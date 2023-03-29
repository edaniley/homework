#ifndef ORDER_STATE_MANAGEMENT_H_INCLUDED
#define ORDER_STATE_MANAGEMENT_H_INCLUDED

#include <iostream>

#include <hw/Util.h>
#include <hw/FSM.h>

namespace OrderStateManagment {

struct NewOrder {
  double price;
  int quantity;
};
struct AckOrder {};
struct RejectOrder {};
struct FillOrder {
  int quantity;
};
struct CancelOrder {
  int quantity;
};
struct ModifyOrder {
  double price;
  int quantity;
};

template<typename T> // this is not necessary for Dispatch; to facilitate debugging
struct OrderStateBase {
  OrderStateBase() { std::cout << "+ " << hw::TypeName<T>() << "()" << std::endl; }
  OrderStateBase(const OrderStateBase<T> &) { std::cout << "+ " << hw::TypeName<T>() << "(const &)" << std::endl; }
  OrderStateBase(OrderStateBase<T> &&) { std::cout << "+ " << hw::TypeName<T>() << "(&&)" << std::endl; }
  ~OrderStateBase() {std::cout << "- " << hw::TypeName<T>() << std::endl; }
  //OrderStateBase& operator=(OrderStateBase&& data) = default;
  OrderStateBase& operator=(OrderStateBase&& data) {
    std::cout << "= " << hw::TypeName<T>() << "(&&)" << std::endl;
    return *this;
  }
};

struct OrderStateNew : OrderStateBase<OrderStateNew> {
  OrderStateNew() { std::cout << "New order\n"; }
};

struct OrderStateLive : OrderStateBase <OrderStateLive> {
  OrderStateLive() { std::cout << "Order live" << '\n'; }
};
struct OrderStateRejected : OrderStateBase <OrderStateRejected> {
  OrderStateRejected() { std::cout << "Order rejected" << '\n'; }
};

struct OrderStateFilled: OrderStateBase <OrderStateFilled> {
  OrderStateFilled() { std::cout << "Order filled" << '\n'; }
};

struct OrderStateCanceled : OrderStateBase<OrderStateCanceled>{
  OrderStateCanceled() { std::cout << "Order cancelled\n"; }
};

struct OrderStateOverFilled : OrderStateBase <OrderStateOverFilled> {
  OrderStateOverFilled() { std::cout << "Order over filled\n"; }
};

using state = std::variant<OrderStateNew,// must be first alternative ??
  OrderStateLive, OrderStateRejected,OrderStateFilled, OrderStateOverFilled, OrderStateCanceled>;

///////////////////////////////////////////////////////////////////////////////
struct OrderData {
  int ord_qty;
  int done_qty;
  int cxl_qty;
};

///////////////////////////////////////////////////////////////////////////////
class OrderState : public hw::FiniteStateMachine<OrderState, state> {
  using TParent = hw::FiniteStateMachine<OrderState, state>;
  OrderData m_ord;
public:
  explicit OrderState(int quantity) : TParent(OrderStateNew{}) {
    m_ord.ord_qty = quantity;
    m_ord.done_qty = 0;
    m_ord.cxl_qty = 0;
    std::cout << "Order size:" << quantity << std::endl;
  }

  template<typename State, typename Event>
  auto OnEvent(State&, const Event&) {
    std::cout << "Invalid transition"
          " state:" << hw::TypeName<State>() <<
          " transaction:" << hw::TypeName<Event>() << std::endl;
    return std::nullopt;
  }

  auto OnEvent(OrderStateNew&, const AckOrder& e) { return OrderStateLive {}; }
  auto OnEvent(OrderStateNew&, const RejectOrder& e) { return OrderStateRejected{}; }
  auto OnEvent(OrderStateLive&, const CancelOrder & e) {
    const int leaves_qty = m_ord.ord_qty - m_ord.done_qty;
    if (e.quantity == -1 || e.quantity <= leaves_qty) {
      m_ord.ord_qty -= e.quantity == -1 ? leaves_qty : e.quantity;
      if (m_ord.ord_qty) {
        std::cout << "Order size reduced to " << m_ord.ord_qty << std::endl;
      } else {
        std::cout << "Order canceled" << std::endl;
      }
      return m_ord.ord_qty > m_ord.done_qty ? std::nullopt : std::optional<state>(OrderStateCanceled{});
    } else {
      std::cout << "Invalid canceled request" << std::endl;
    }
    return std::optional<state>{};
  }
  auto OnEvent(OrderStateLive&, const FillOrder& e) {
    m_ord.done_qty += e.quantity;
    std::cout << (m_ord.done_qty < m_ord.ord_qty ? "Filled " : "Overfilled ") << e.quantity
         << " leaves: " << (m_ord.ord_qty - m_ord.done_qty) << std::endl;
    return m_ord.done_qty < m_ord.ord_qty ? std::nullopt :
           m_ord.done_qty == m_ord.ord_qty ? std::optional<state>(OrderStateFilled{}) : std::optional<state>(OrderStateOverFilled{});
  }
  auto OnEvent(OrderStateFilled&, const FillOrder& e) {
    m_ord.done_qty += e.quantity;
    std::cout << "Overfiled " << e.quantity << " leaves: " << (m_ord.ord_qty - m_ord.done_qty) << std::endl;
    return OrderStateOverFilled{};
  }
  auto OnEvent(OrderStateOverFilled&, const FillOrder& e) {
    m_ord.done_qty += e.quantity;
    return std::nullopt;
  }
  auto OnEvent(OrderStateOverFilled&, const CancelOrder& e) {
    std::cout << "Cannot cancel filled order" << std::endl;
    return std::nullopt;
  }

};

}

using namespace OrderStateManagment;

static void test_Fill() {
  OrderState order_state(10000);
  order_state.Dispatch(AckOrder{});
  order_state.Dispatch(FillOrder{ 2000 });
  order_state.Dispatch(FillOrder{ 4000 });
  order_state.Dispatch(FillOrder{ 4000 });
}

static void test_PartialFill() {
  OrderState order_state(10000);
  order_state.Dispatch(AckOrder{});
  order_state.Dispatch(FillOrder{ 2000 });
  order_state.Dispatch(FillOrder{ 4000 });
  order_state.Dispatch(CancelOrder{ -1 });
}

static void test_Overfill() {
  OrderState order_state(10000);
  order_state.Dispatch(AckOrder{});
  order_state.Dispatch(FillOrder{ 2000 });
  order_state.Dispatch(FillOrder{ 4000 });
  order_state.Dispatch(CancelOrder{ 1000 });
  order_state.Dispatch(FillOrder{ 4000 });
  order_state.Dispatch(CancelOrder{ -1 });
}

static void test_Invalid() {
  OrderState order_state(10000);
  order_state.Dispatch(FillOrder{ 2000 });
  order_state.Dispatch(RejectOrder{ });
}

inline
void test_OrderStateManagment() {
  try {
    test_Fill();
    test_PartialFill();
    test_Overfill();
    test_Invalid();
  }
  catch (const std::exception& ex) {
    std::cerr << "Unhandled std exception caught: " << ex.what() << '\n';
  }
  catch (...) {
    std::cerr << "Unhandled unknown exception caught\n";
  }
}


#endif
