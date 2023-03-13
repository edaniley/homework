#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <map>
#include <bit>

template<typename T>
T BigEndianToHost (T val) {
//  if constexpr (std::endian::native == std::endian::little) { //-std=c++20
    if constexpr (sizeof(T) == 2) {
      return __builtin_bswap16(val);
    } else if constexpr (sizeof(T) == 4) {
      return __builtin_bswap32(val);
    } else if constexpr (sizeof(T) == 8) {
      return __builtin_bswap64(val);
    }
//  }
  return val;
}

struct __attribute__((packed)) PktHeader {
  uint16_t m_streamID;
  uint32_t m_packetLength;
  uint32_t StreamID()     const { return BigEndianToHost(m_streamID); }
  uint32_t PacketLength() const { return BigEndianToHost(m_packetLength); }
};

struct __attribute__((packed)) MsgHeader {
  static constexpr size_t MAX_MSG_SIZE = 82;
  static constexpr size_t EXEC_SHARES_OFFSET = 26;

  uint16_t  m_messageLength;
	char      m_packetType;
	char      m_messageType;

	size_t FullSize() const {
	  switch (m_messageType) {
	    case 'S': return 13;
	    case 'A': return 68;
	    case 'U': return 82;
	    case 'C': return 31;
	    case 'E': return 43;
	    default: break;
	  }
	  assert(false);
	  return 0;
	}

	uint32_t  ExecutedShares() const {
	  return BigEndianToHost(*(uint32_t *)((uint8_t *)this + EXEC_SHARES_OFFSET));
	}
};

struct Counters {
  size_t m_syseventCnt = 0;
  size_t m_acceptedCnt = 0;
  size_t m_canceledCnt = 0;
  size_t m_replacedCnt = 0;
  size_t m_executedCnt = 0;
  size_t m_executedVol = 0;
};

struct Stream {
  Counters  m_counters;
  size_t    m_msgLength = 0;
  char      m_msg[MsgHeader::MAX_MSG_SIZE];

	void AccumulateCounters() {
	  const MsgHeader & hdr = *(MsgHeader*)m_msg;
    switch (hdr.m_messageType) {
      case 'S': ++m_counters.m_syseventCnt; break;
      case 'A': ++m_counters.m_acceptedCnt; break;
      case 'U': ++m_counters.m_replacedCnt; break;
      case 'C': ++m_counters.m_canceledCnt; break;
      case 'E':
        ++m_counters.m_executedCnt;
        m_counters.m_executedVol += hdr.ExecutedShares();
        break;
      default:
        break;
    }
	}

  void ProcessPacket(std::ifstream &ifs, size_t pktlen) {
    assert(m_msgLength + pktlen <= sizeof(m_msg));
    ifs.read(m_msg + m_msgLength, pktlen);
    if (pktlen == (size_t)ifs.gcount()) {
      m_msgLength += pktlen;
      if (m_msgLength >= sizeof(MsgHeader)) {
        const MsgHeader &msghdr = *(MsgHeader *)m_msg;
        assert(m_msgLength <= msghdr.FullSize());
        if (m_msgLength == msghdr.FullSize()) {
          AccumulateCounters();
          m_msgLength = 0;
        }
      }
    }
  }

};

void PrintCounters(const std::string &caption, const Counters &counters) {
  std::cout << caption << std::endl;
  std::cout << " Accepted: " << counters.m_acceptedCnt << " messages" << std::endl;
  std::cout << " System Event: " << counters.m_syseventCnt << " messages" << std::endl;
  std::cout << " Replaced: " << counters.m_replacedCnt << " messages" << std::endl;
  std::cout << " Canceled: " << counters.m_canceledCnt << " messages" << std::endl;
  std::cout << " Executed: " << counters.m_executedCnt << " messages: "
      << counters.m_executedVol << " shares" << std::endl << std::endl;
}

int rbc_main(int argc, char **argv) {
  const char *path = argc> 1 ? argv[1] : "OUCHLMM2.incoming.packets";
  std::map<uint16_t, Stream> streams;
	std::ifstream datafile;
	PktHeader pkthdr;

	datafile.open(path, std::ios::binary|std::ios::in);
	while (datafile.good()) {
    datafile.read((char *)&pkthdr, sizeof(PktHeader));
    if (datafile.gcount() == sizeof(PktHeader)) {
		  streams[pkthdr.StreamID()].ProcessPacket(datafile, pkthdr.PacketLength());
		}
  }

	Counters totals;
	for (const auto &entry : streams) {
	  std::ostringstream os;
	  os << "Stream " << entry.first;
	  const Counters & counters = entry.second.m_counters;
	  PrintCounters(os.str(), counters);
	  totals.m_acceptedCnt += counters.m_acceptedCnt;
	  totals.m_syseventCnt += counters.m_syseventCnt;
	  totals.m_replacedCnt += counters.m_replacedCnt;
	  totals.m_canceledCnt += counters.m_canceledCnt;
	  totals.m_executedCnt += counters.m_executedCnt;
	  totals.m_executedVol += counters.m_executedVol;
	}
	PrintCounters("Totals:", totals);
	return 0;
}
