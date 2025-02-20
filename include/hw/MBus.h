#pragma once

#include <exception>
#include <atomic>
#include <chrono>
#include <unordered_map>

#include "Util.h"
#include "Struct.h"

namespace hw {

  enum IOType {INPUT, OUTPUT};
  struct Input {
    static constexpr IOType IOTYPE = INPUT;
  };

  struct Output {
    static constexpr IOType IOTYPE = OUTPUT;
  };

  // ETHER MSG //////////////////////////////////////////////////////////////////
  template <typename TEtherTraits>
  struct alignas(CACHE_LINE_SIZE) EtherMsg
  {
    using TMsgList = typename TEtherTraits::MSG_LIST;
    using TMsgTypesOptions = mp::transform_t<TMsgList, std::add_pointer>::VARIANT;

    struct Info {
      std::atomic<uint64_t> m_seqno = 0;
      uint64_t              m_timestamp = 0;
      size_t                m_id = (size_t)-1; // to remove; use TMsgTypesOptions varint
      TMsgTypesOptions      m_msgtypes;
      // replace std::chrono::system_clock with CPU cycle based solution
      static_assert (sizeof(std::chrono::system_clock::time_point) == sizeof(m_timestamp));
    };

    template <size_t I = 0>
    static constexpr size_t MaxSize() {
      if constexpr (false == mp::empty_v<TMsgList> && I < mp::size_of_v<TMsgList>) {
        using TType = mp::at_t<TMsgList, I>;
        return std::max(sizeof(typename TType::type), MaxSize<I+1>());
      }
      else return 0;
    }

    static constexpr size_t MaxDataSize() {
      return MaxSize<0>();
    }

    static constexpr size_t AllocSize() {
      const size_t ret = MaxDataSize() + sizeof(Info);
      const size_t rem = ret % CACHE_LINE_SIZE;
      return rem ? ret + (CACHE_LINE_SIZE - rem) : ret;
    }

    char m_data[AllocSize()];
    Info m_info;

    template< typename T>
    void SetTypeID(T *t = nullptr) {
      m_info.m_msgtypes = t;
      m_info.m_id = T::ID;
    }

    size_t CurrentTypeID()  const { return m_info.m_id; }//m_msgtypes.index(); }
    size_t SeqNo()          const { return m_info.m_seqno.load(std::memory_order_relaxed); }

    template< typename T>
    T * Cast() { return reinterpret_cast<T *>(this); }

    template< typename T>
    const T * Cast() const { return reinterpret_cast<const T *>(this); }
  };

  // ETHER //////////////////////////////////////////////////////////////////////
  template<typename TraitsT, size_t ETHER_SIZE>
  struct Ether   {
    using TTraits           = TraitsT;
    using TMsgList          =  typename TTraits::MSG_LIST;
    using TMsgTypesOptions  = mp::transform_t<TMsgList, std::add_pointer>::VARIANT;
    using TMsg              = EtherMsg<TTraits>;

    static constexpr size_t SIZE = ETHER_SIZE;
    static constexpr size_t MSG_TYPE_CNT = mp::size_of_v<TTraits::MSG_LIST>;

    TMsg                                    m_msg[SIZE];
    std::atomic<size_t>                     m_seqno = 0;
    std::unordered_map<std::string, size_t> m_idByName;

    Ether() {
      mp::for_each<TMsgList>([this] (size_t i, auto* ptype) {
        using TType = std::remove_pointer_t<decltype(ptype)>;
        m_idByName[TType::Name()] = i;
      });
    }

    size_t NameToID(const char *name) const {
      // to be used when converting from text
      const auto it = m_idByName.find(name);
      return (it != m_idByName.end()) ? it->second : (size_t)-1;
    }

    TMsg* AllocMsg() noexcept {
      size_t seqno = m_seqno.load(std::memory_order_relaxed);
      while (!m_seqno.compare_exchange_weak(
          seqno, seqno+1, std::memory_order_release, std::memory_order_relaxed));
      ++seqno;
      TMsg &msg =  m_msg[seqno % SIZE];//replace with bitwise
      msg.m_info.m_timestamp = 0;
      msg.m_info.m_seqno.store(seqno, std::memory_order_release);// maybe use seqno as commit no  instead of m_timestamp?
      // use n_allocno as current m_seqno
      // copy n_allocno to commit message content
      // n_allocno == m_seqno indicates that message is ready for consumption
      return &msg;
    }

    template <typename T>
    void CommitMsg(TMsg &msg) noexcept {
      const auto ts = std::chrono::system_clock::now();
      msg.m_info.m_msgtypes = (T)0;
      msg.m_info.m_timestamp = *(uint64_t*)&ts;
      // copy newly allocated seqno to commited seqno
    }

    TMsg* GetMsg(size_t expectedSecno) {
      TMsg &msg = m_msg[expectedSecno % SIZE];
      auto seqno = msg.m_info.m_seqno.load(std::memory_order_acquire);
      if (seqno < expectedSecno)
        return nullptr;
        // simplify : use seqno (maybe we need 2: one to assign , another to commit )
        // commited one is examined by GetMsg
        // if less then return nullptr, if as expected then return   more - excepption
      else if (seqno == expectedSecno) {
        // spin here
        return msg.m_info.m_timestamp ? &msg : nullptr;
      }
      throw(std::out_of_range("Iter: tail is overwritten"));
      return nullptr;
    }
  };

  // ETHER ITERATOR /////////////////////////////////////////////////////////////
  template<typename EtherT, typename ComponentsMsgListT>
  struct Etherator {
    using TEther              = EtherT;
    using TMsg                = TEther::TMsg;
    using TComponentsMsgList  = ComponentsMsgListT;
    static constexpr size_t SIZE = TEther::SIZE;
    Etherator(TEther &ether) : m_ether(ether) {}

    TEther &m_ether;
    size_t m_readSeqNo = 0;

    template <typename MsgT>
    struct MsgTypeComponenToEther {
      template <typename T, typename SLIST>
      struct check_head {
        using H = mp::front_t<SLIST>;
        using U = mp::at_t< H , 0>;
        using type = std::conditional_t<
              std::is_same_v<U, T>,
              std::add_pointer_t< mp::at_t< H , 1> >,
              typename check_head<T, mp::pop_front_t<SLIST> >::type
        >;
      };
      template <typename T>
      struct check_head<T, mp::type_list<>>{
        using type = std::monostate;
      };
      using type = check_head<MsgT, TComponentsMsgList>::type;
    };

    TEther & GetEther() const { return m_ether; };

    auto NextMsg() {
      TMsg *msg = m_ether.GetMsg(m_readSeqNo+1);
      m_readSeqNo += (bool)msg;;
      return msg;
    }

    template <typename T>
    T * AllocMsg() {
      T *t = m_ether.AllocMsg()->template Cast<T>();
      return new (t) T();
    }

    template <typename MsgT>
    void CommitMsg(MsgT &msg)  {
      using T = MsgTypeComponenToEther<MsgT>::type;
      static_assert(mp::has_alternative_v< typename TMsg::TMsgTypesOptions, T>);
      m_ether.template CommitMsg<T>(reinterpret_cast<TMsg &>(msg));
    }
  };
} // namespace hw
