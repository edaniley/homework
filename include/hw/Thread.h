#pragma once

#include "MP.h"
#include "MBus.h"

namespace hw {


template< typename TThread, typename TraitsT, typename EtherT>
struct ThreadBase {
  using TTraits         = TraitsT;
  using TEther          = EtherT;
  using TSelf           = ThreadBase<TThread, TTraits, TEther>;
  using TEtherTraits    = typename TEther::TTraits;
  using TEtherMsg       = typename TEther::TMsg;
  using TEtherMsgList   = typename TEtherTraits::MSG_LIST;
  using TComponentList  = typename TTraits::COMPONENT_LIST;
  using TComponents     = typename mp::transform_t<TComponentList, mp::add_unique_ptr>::TUPLE; // std::tuple<std::unique_ptr<Component>,...>;

  template<typename T> struct CallbackOptionsFromComponent { using type = typename T::TCallbackOptions; };
  using TCallbackOptions  = typename mp::transform_t<TComponentList, CallbackOptionsFromComponent>::VARIANT;
  using TCallbackMap      = std::array<std::vector<TCallbackOptions>, mp::size_of_v<TEtherMsgList>>;

  // create combined list of components' message types and corresponding ether messages
  template<typename T> struct MsgListFromComponent {
    using type = mp::merge_t<typename T::TComponentMsgList, typename T::TEtherMsgList>;
  };
  template <typename ComponentListT>
  struct CombineMsgLists {
    template<typename DLIST, typename SLIST>
    struct combine {
      using new_list = MsgListFromComponent<mp::front_t<SLIST>>::type;
      using dst_list = mp::add_t<DLIST, new_list>;
      using src_list = mp::pop_front_t<SLIST>;
      using type = combine<dst_list, src_list>::type;
    };
    template<typename DLIST>
    struct combine<DLIST, mp::type_list<>> {
      using type = DLIST;
    };
    using type = combine<mp::type_list<>, ComponentListT>::type;
  };
  using TMsgList = CombineMsgLists<TComponentList>::type;
  using TEtherator = hw::Etherator<TEther, TMsgList>;

  TThread &             m_thread;
  TEther &              m_ether;
  TEtherator            m_etherator;

  TComponents           m_components;
  TCallbackMap          m_callbacks;
  bool                  m_exit = false;

  template<typename ComponentT>
  void AssingCallbacks(ComponentT & component) {
    for (auto &var: component.m_handlers) {
      const auto id = var.first;
      std::visit([this, id] (auto &cb) {
        m_callbacks[id].push_back(cb);
      }, var.second);
    }
  }

  ThreadBase(TEther &ether)
    : m_thread(static_cast<TThread &>(*this)), m_ether(ether), m_etherator(ether) {
    mp::tuple_for_each<TComponents>(m_components, [this] (auto &ptr) {
      using T =  std::decay_t<decltype(ptr)>::element_type;
      ptr.reset(new T());
      AssingCallbacks(*ptr.get());
    });
  }

  void Dispatch(const TEtherMsg &msg) {
    for (auto &thrdopt : m_callbacks[msg.CurrentTypeID()]) {
      std::visit([&msg] (auto &compopt) {
        std::visit([&msg] (auto &cb) {
          cb.template Call(msg);
        }, compopt);
      }, thrdopt);
    };
  }

  void Run() {
    // in the loop
    // read next message from ether
    // cast to correct payload abd forward const ref to every component
    auto cb = [](size_t i, auto* ptype) {
      using TType = std::remove_pointer_t<decltype(ptype)>;
      // call TType::OnMessage
      std::cout << hw::TypeName<TType>() << std::endl;
    };
    mp::for_each<TComponentList>(cb);
  }

  void Init() {
  }
  void Fini() {
    m_thread.OnCleanUp();
  }
};


}
