
#include <iostream>
#include <bitset>
#include <memory>

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>

#ifndef HW_UNIT_TEST
#define HW_UNIT_TEST
#endif

#include <TimeUtil.h>
//#include <hw/CBuffer.h>
//#include <OrderStateManagment.h>
#include <hw/String.h>
#include <hw/Struct.h>
#include <hw/Component.h>
#include <hw/Thread.h>
#include <hw/Alloc.h>
#include <queue>

using namespace std;


//template <typename T>
//void PrintBits(T val) {
//    cout << bitset<sizeof(size_t)*8>((size_t)val) << " val:"<< ((size_t)val) << endl;
//}
//const size_t PAGE_SIZE = getpagesize();

HW_DEF_STRUCT(Init,
    (int, Mode, 0)
    );

HW_DEF_STRUCT(OrderData,
    (uint32_t , OrdSize, 99)
    (double , LimitPx, 11.99)
    );

//HW_DEF_STRUCT(Bid,
//    (hw::String<5>, BidExch, "T")
//    (double , BidPrice, 0.0)
//    (uint32_t , BidSize, 0)
//    );

struct Bid: hw::StructBase < Bid > {
  using STRUCT_LIST = mp::type_list < Bid > ;
  using FIELD_LIST  = mp::type_list <
    hw::Field < hw::String < 5 > ,  "BidExch" > ,
    hw::Field < double,  "BidPrice" > ,
    hw::Field < uint32_t,  "BidSize" >
  > ;

  static const size_t STRUCT_CNT = 1;
  static  const size_t FIELD_CNT = 3;

  static_assert(mp::is_unique_v < FIELD_LIST > );
  static_assert(mp::is_unique_v < STRUCT_LIST > );

  Bid() : m_BidExch("T"),
          m_BidPrice(0.0),
          m_BidSize(0) {}

  Bid(  hw::String < 5 > a_BidExch, double a_BidPrice, uint32_t a_BidSize): m_BidExch(a_BidExch),  m_BidPrice(a_BidPrice),  m_BidSize(a_BidSize) {}

  hw::Field < hw::String < 5 > ,  "BidExch" > m_BidExch;
  operator hw::Field < hw::String < 5 > ,  "BidExch" > & () {
    return m_BidExch;
  }
  operator  const hw::Field < hw::String < 5 > , "BidExch" > & () const {
    return m_BidExch;
  }
  hw::String < 5 > & BidExch() {
    return m_BidExch.m_val;
  }
  const hw::String < 5 > & BidExch() const {
    return m_BidExch.m_val;
  }

  hw::Field < double,  "BidPrice" > m_BidPrice;
  operator hw::Field < double, "BidPrice" > & () {
    return m_BidPrice;
  }
  operator const hw::Field < double, "BidPrice" > & () const {
    return m_BidPrice;
  }
  double & BidPrice() {
    return m_BidPrice.m_val;
  }
  const double & BidPrice() const {
    return m_BidPrice.m_val;
  }

  hw::Field < uint32_t,  "BidSize" > m_BidSize;
  operator hw::Field < uint32_t,  "BidSize" > & () {
    return m_BidSize;
  }
  operator  const hw::Field < uint32_t, "BidSize" > & () const {
    return m_BidSize;
  }
  uint32_t & BidSize() {
    return m_BidSize.m_val;
  }
  const uint32_t & BidSize() const {
    return m_BidSize.m_val;
  }
};

HW_DEF_STRUCT(Offer,
    (hw::String<5>, OfferExch, "T")
    (double , OfferPrice, 999999.99)
    (uint32_t , OfferSize, 0)
    );

HW_CAT_STRUCT((Quote, Bid, Offer));
HW_CAT_STRUCT((Order, OrderData, Quote));

/////////////////////////////////////////////////
//Quoter.h
namespace md {
  struct Init  : ::Init,  hw::Input {};
  struct Bid   : ::Bid,  hw::Input {};
  struct Offer : ::Offer,  hw::Input {};
  struct Quote : ::Quote,  hw::Output {};

  template<typename TraitsT, typename EtherT>
  struct QuoterComponent : hw::ComponentBase<QuoterComponent<TraitsT, EtherT>, TraitsT> {
    void OnMsg(const Init &msg) {
      std::cout << "Quoter OnMsg md::Init" << std::endl;
    }
    void OnMsg(const Bid &msg) {
      std::cout << "Quoter OnMsg md::Bid" << std::endl;
    }
    void OnMsg(const Offer &msg) {
      std::cout << "Quoter OnMsg md::Offer" << std::endl;
    }
  };
} // md

//Trader.h
namespace tr {
  struct Init  : ::Init,   hw::Input {};
  struct Quote : ::Quote,  hw::Input {};
  struct Order : ::Order,  hw::Output {};

  template<typename TraitsT, typename EtherT>
  struct TraderComponent : hw::ComponentBase<TraderComponent<TraitsT, EtherT>, TraitsT> {

    void OnMsg(const Init &msg) {
      std::cout << "Trader OnMsg tr::Init" << std::endl;
    }

    void OnMsg(const Quote &msg) {
      std::cout << "Trader OnMsg tr::Quote" << std::endl;
    }
  };

} //namespace tr

///////////////////////////////////////////////////// in CPP
// Skeleton.cpp
// use DEF_ETHER_TRAITS to define messages and traits
// instantiate ether template
struct InitMsg : hw::NamedStruct<Bid, "INIT"> {
  static constexpr size_t ID = 0;
};
struct BidMsg : hw::NamedStruct<Bid, "MD/BID"> {
  static constexpr size_t ID = 1;
};
struct OfferMsg : hw::NamedStruct<Offer, "MD/OFFER"> {
  static constexpr size_t ID = 2;
};
struct QuoteMsg : hw::NamedStruct<Quote, "MD/QUOTE"> {
  static constexpr size_t ID = 3;
};
struct OrderMsg : hw::NamedStruct<Order, "MD/ORDER"> {
  static constexpr size_t ID = 4;
};
struct EtherTraits {
  using MSG_LIST = mp::type_list<InitMsg, BidMsg, OfferMsg, QuoteMsg, OrderMsg>;
};

using TEther = hw::Ether<EtherTraits, 10>;

// define component traits and instantiate component templates
struct QuoterComponentTraits {
  using COMPONENT_MSG_LIST  = mp::type_list< md::Init, md::Bid, md::Offer, md::Quote >;
  using ETHER_MSG_LIST      = mp::type_list< InitMsg, BidMsg, OfferMsg, QuoteMsg >;
  using ETHER_TYPE          = TEther;
};
using TQuoter = md::QuoterComponent< QuoterComponentTraits, EtherTraits>;

struct TraderComponentTraits {
  using COMPONENT_MSG_LIST  = mp::type_list< tr::Init, tr::Quote, tr::Order >;
  using ETHER_MSG_LIST      = mp::type_list< InitMsg, QuoteMsg, OrderMsg >;
  using ETHER_TYPE          = TEther;
};
using TTrader = tr::TraderComponent< TraderComponentTraits, EtherTraits>;

// define thread traits and instantiate thread templates
struct ProducerTraits {
  using COMPONENT_LIST  = mp::type_list<TQuoter>;
};

struct Producer : hw::ThreadBase<Producer, ProducerTraits, TEther> {
  using TTraits = ProducerTraits;
  void OnCleanUp() { cout << "OnCleanUp " << hw::TypeName<decltype(*this)>() << endl; }
};

struct ConsumerTraits {
  using COMPONENT_LIST  = mp::type_list<TTrader>;
};

struct Consumer : hw::ThreadBase<Consumer, ConsumerTraits, TEther> {
  using TTraits = ConsumerTraits;
  void OnCleanUp() { cout << "OnCleanUp " << hw::TypeName<decltype(*this)>() << endl; }
};

struct ProducerConsumerTraits {
  using COMPONENT_LIST  = mp::type_list<TQuoter, TTrader>;
};

struct ProducerConsumer : hw::ThreadBase<ProducerConsumer, ProducerConsumerTraits, TEther> {
  using TTraits = ProducerConsumerTraits;
  void OnCleanUp() { cout << "OnCleanUp " << hw::TypeName<decltype(*this)>() << endl; }
};


static std::string tag("987654321-00797098707908790");
[[maybe_unused]] static void __attribute__((noinline)) test()  {
//  {
//    test_Alloc({1,2,3,4,5});
//  }
  //    [[maybe_unused]]
  // test basic struct copy
  Bid bid;
  bid.BidExch() = "A";
  bid.BidPrice() = 12.12;
  bid.BidSize() = 200;

  Offer offer;
  offer.OfferExch() = "N";
  offer.OfferPrice() = 12.34;
  offer.OfferSize() = 300;

  Quote qoute;
  qoute.CopyFrom(bid);
  qoute.CopyFrom(offer);
  cout << "Quote: " << qoute.ToString() << endl;

  using T = hw::Field < hw::String < 6 > ,  "BidExch" >;
  [[maybe_unused]]  T exch("AAA");
  //cout << "bid:" << exch << endl;
  [[maybe_unused]]  hw::String<17> s(tag);
  [[maybe_unused]]  hw::String<16> x;
  x = s;
  [[maybe_unused]]  char s10[10];
  const char *t = "12345";
  //std::string s ;
  s = tag;
  cout << "S:" << s << endl;
  s = t;
  exit(0);
}


int main(int argc, char **argv) {
  test();
  [[maybe_unused]] TQuoter quoter;
  [[maybe_unused]] EtherTraits et;
  [[maybe_unused]] TEther ether;
  std::cout << "MaxDataSize:" << TEther::TMsg::MaxDataSize() << std::endl;
  std::cout << "AllocSize:" << TEther::TMsg::AllocSize() << std::endl;
  std::cout << "test id: " << ether.NameToID("MD/QUOTE") << std::endl;

  [[maybe_unused]] TEther::TMsg msg;
  [[maybe_unused]] ProducerConsumer prodcons(ether);
  [[maybe_unused]] Producer producer(ether);
  [[maybe_unused]] Consumer consumer(ether);

  auto & bid = *prodcons.m_etherator.AllocMsg<md::Bid>();
  bid.BidPrice() = 123.55;
  bid.BidSize() = 99;
  bid.BidExch() = "NASDAQ or Arca";
  auto & quote = *prodcons.m_etherator.AllocMsg<tr::Quote>();
  quote.CopyFrom(bid);
  std::cout << bid.ToString() << std::endl;
  std::cout << quote.ToString() << std::endl;
  std::cout << "TypeListToString:" << hw::TypeListToString<md::Bid>("/", " ") << std::endl;
  std::cout << "Type: " << hw::TypeName< decltype(bid) >() << std::endl;
  prodcons.m_etherator.CommitMsg(bid);

    //for (size_t id = 0;id < 6; ++id) {
    msg.SetTypeID<InitMsg>();
    prodcons.Dispatch(msg);
    producer.Dispatch(msg);
    consumer.Dispatch(msg);
  //}
  //[[maybe_unused]] BidMsg bid;
  producer.Fini();
  producer.Run();
  consumer.Run();



  //  std::cout << "rdtsc:" << hw::rdtsc() << std::endl;
  return 0;
}
