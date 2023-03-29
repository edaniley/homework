#pragma once

#include "MP.h"
#include "MBus.h"

namespace hw {


template<typename ComponentT, typename TraitsT>
struct ComponentBase {
  using TComponent        = ComponentT;
  using TTraits           = TraitsT;
  using TEther            = typename TTraits::ETHER_TYPE;
  //using TEtherTraits      = typename TEther::TTraits;
  //using TEtherMsg         = typename TEther::TMsg;
  using TComponentMsgList = typename TTraits::COMPONENT_MSG_LIST; // component::Msg1,...
  using TEtherMsgList     = typename TTraits::ETHER_MSG_LIST;     // ether::Msg1..

  template <typename MsgT, typename EtherMsgT>
  struct OnMsgCaller {
    static constexpr auto ID = EtherMsgT::ID;
    TComponent &m_component;

    OnMsgCaller(TComponent &component) : m_component(component) {}
    ~OnMsgCaller() = default;

    template<typename T>
    void Call(const T &msg) {
      auto ptr = msg.template Cast<MsgT>();
      m_component.OnMsg(*ptr);
    }
  };

  template <typename T> struct InputMsgFilter { static constexpr bool value = mp::at_t<T,0>::IOTYPE == INPUT; };
  template <typename T> struct MsgTypeCaller  { using type = OnMsgCaller< mp::at_t<T,0>, mp::at_t<T,1> >; };
  using TInputMsgList     = mp::filter_t< mp::merge_t<TComponentMsgList, TEtherMsgList>, InputMsgFilter >;
  using TCallbackOptions  = mp::transform_t< TInputMsgList, MsgTypeCaller >::VARIANT;

  ComponentT &m_component;
  std::shared_ptr<TEther> m_ether;
  std::vector<std::pair<size_t, TCallbackOptions>> m_handlers;

  ComponentBase() : m_component(*static_cast<TComponent *>(this)) {
    mp::for_each<TInputMsgList>([this] (size_t idx, auto* ptype) {
        using T = std::remove_pointer_t<decltype(ptype)>;
        using TCompMsg = mp::at_t<T, 0>;
        using TEthrMsg = mp::at_t<T, 1>;
        OnMsgCaller< TCompMsg, TEthrMsg > cb(m_component);
        m_handlers.emplace_back(TEthrMsg::ID, TCallbackOptions{cb});
      }
    );
  }

  void Init () {};
  void Fini () {};
};






}
