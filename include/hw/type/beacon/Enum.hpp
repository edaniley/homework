// --- START FILE: include/hw/type/beacon/Enum.hpp ---
#pragma once

#include <utility>
#include <ostream>
#include <string_view>
#include <string>
#include <concepts>
#include <cstring>
#include <better-enum/enum.h>

#include <hw/type/NameTag.hpp>
#include <hw/type/beacon/TypeTraits.hpp>
#include <hw/utility/Format.hpp>

namespace hw::type::beacon {

template<typename T>
concept BetterEnumType = requires(T t) {
  { T::_size() } -> std::same_as<size_t>;
  { t._to_string() } -> std::convertible_to<const char*>;
  { T::_from_string_nothrow("string") } -> std::same_as<better_enums::optional<T>>;
  { T::_from_integral_nothrow(0) } -> std::same_as<better_enums::optional<T>>;
  typename T::_integral;
};

template <typename T>
struct IsBetterEnumType : std::false_type {};

template <typename T>
requires BetterEnumType<T>
struct IsBetterEnumType<T> : std::true_type {};

template<NameTag Tag, BetterEnumType ValueType>
struct NamedEnumType {
  using type_trait = trait::Enum;
  using value_type = ValueType;
  using integral_type = typename ValueType::_integral;
  static constexpr size_t SIZE = sizeof(integral_type);
  static constexpr NameTag name_tag = Tag;

  static constexpr size_t size(const std::byte*) {
    return SIZE;
  }

  template <typename Type>
  requires std::convertible_to<Type, integral_type>
  static void set(std::byte* ptr, Type&& value) {
    auto opt = ValueType::_from_integral_nothrow(static_cast<integral_type>(std::forward<Type>(value)));
    if (!opt) {
      throw std::invalid_argument(frmt::format("Invalid integral value for Enum {}: {}", Tag.toString(), value));
    }
    ValueType val = *opt;
    std::memcpy(ptr, &val, SIZE);
  }

  static void set(std::byte* ptr, std::string_view value) {
    // Better Enums _from_string requires null-termination; std::string ensures this.
    auto opt = ValueType::_from_string_nothrow(std::string(value).c_str());
    if (!opt) {
      throw std::invalid_argument(frmt::format("Invalid string value for Enum {}: '{}'", Tag.toString(), value));
    }
    ValueType val = *opt;
    std::memcpy(ptr, &val, SIZE);
  }

  static ValueType get(const std::byte* ptr) {
    ValueType val = ValueType::_from_integral(0);
    std::memcpy(&val, ptr, SIZE);
    return val;
  }

  static std::string toString(const std::byte* ptr) {
    return get(ptr)._to_string();
  }
};

} // namespace hw::type::beacon
// --- END FILE: include/hw/type/beacon/Enum.hpp ---
