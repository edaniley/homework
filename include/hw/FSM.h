#pragma once

#include <variant>
#include <optional>

// Finite state machine
namespace hw {

template <typename TState, typename TStateVar>
class FiniteStateMachine {
public:
  FiniteStateMachine(const TStateVar& currenState) : current_state_(currenState) {}

  template<typename TEvent>
  void Dispatch(TEvent && event) {
    TState& state = static_cast<TState&>(*this);
    auto new_state = std::visit(
      [&] (auto & s) ->std::optional<TStateVar> {
        return state.OnEvent(s, std::forward<TEvent>(event));
      },
      current_state_
    );
    if (new_state) {
      current_state_ = *std::move(new_state);
    }
  }

  TStateVar & CurrentState()              { return current_state_; }
  const TStateVar & CurrentState() const  { return current_state_; }
private:
  TStateVar current_state_;
};

}
