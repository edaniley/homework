#pragma once

#include <variant>
#include <optional>

// Finite state machine
namespace hw {

template <typename OrderState, typename OrderStateChoices>
class FiniteStateMachine {
public:
  FiniteStateMachine(const OrderStateChoices& currenState) : current_state_(currenState) {}

  template<typename TEvent>
  void Dispatch(TEvent && event) {
    OrderState& state = static_cast<OrderState&>(*this);
    auto new_state = std::visit(
      [&] (auto & s) ->std::optional<OrderStateChoices> {
        return state.OnEvent(s, std::forward<TEvent>(event));
      },
      current_state_
    );
    if (new_state) {
      current_state_ = *std::move(new_state);
    }
  }

  OrderStateChoices & CurrentState()              { return current_state_; }
  const OrderStateChoices & CurrentState() const  { return current_state_; }
private:
  OrderStateChoices current_state_;
};

}
