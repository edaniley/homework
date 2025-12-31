// --- START FILE: include/hw/type/beacon/Numeric.hpp ---
#pragma once

#include <limits>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <ostream>
#include <iomanip>
#include <concepts>
#include <stdexcept>
#include <bit>

#include <boost/multiprecision/cpp_int.hpp>
#include <hw/type/NameTag.hpp>
#include <hw/type/TypeInfo.hpp>
#include <hw/utility/Text.hpp>
#include <hw/utility/Format.hpp>
#include <hw/type/beacon/TypeTraits.hpp>

namespace hw::type::beacon {

// --- Concepts ---

template <typename T>
concept TimeDurationType = requires {
  typename T::duration;
};

template <typename T>
concept LongNumericType = std::is_same_v<T, __int128_t> || std::is_same_v<T, __uint128_t>;

template <typename T>
concept NumericType = (
  std::integral<T> ||
  std::floating_point<T> ||
  std::is_enum_v<T> ||
  TimeDurationType<T> ||
  LongNumericType<T>
);

// --- Helpers ---

template <LongNumericType T>
std::ostream& operator << (std::ostream& os, const T& number) {
  os << boost::multiprecision::int128_t(number);
  return os;
}

// --- NamedNumericType ---

template <NameTag Tag, NumericType ValueType>
struct NamedNumericType {
  using type_trait = trait::Numeric;
  using value_type = ValueType;
  static constexpr size_t SIZE = sizeof(ValueType);
  static constexpr NameTag name_tag = Tag;

  static constexpr size_t size(const std::byte*) {
    return SIZE;
  }

  // Safe Write (Handles Alignment UB)
  template <typename Type>
  requires std::is_assignable_v<ValueType&, Type>
  static void set(std::byte* ptr, Type&& value) {
    ValueType v = static_cast<ValueType>(std::forward<Type>(value));
    // Note: If Beacon is Big-Endian, perform swap here: v = std::byteswap(v);
    std::memcpy(ptr, &v, SIZE);
  }

  // String-based Write
  static void set(std::byte* ptr, std::string_view val) {
    set(ptr, hw::utility::fromString<ValueType>(std::string(val)));
  }

  // Safe Read (Handles Alignment UB)
  static ValueType get(const std::byte* ptr) {
    ValueType v;
    std::memcpy(&v, ptr, SIZE);
    // Note: If Beacon is Big-Endian, perform swap here: v = std::byteswap(v);
    return v;
  }

  static std::string toString(const std::byte* ptr) {
    ValueType val = get(ptr);

    if constexpr (std::is_same_v<ValueType, int8_t> || std::is_same_v<ValueType, uint8_t>) {
      // Improved character vs numeric formatting
      if (std::isalnum(static_cast<unsigned char>(val))) {
        return frmt::format("'{}'", static_cast<char>(val));
      }
      return frmt::format("0x{:02x}", static_cast<uint8_t>(val));
    }
    else if constexpr (LongNumericType<ValueType>) {
      std::ostringstream oss;
      oss << val;
      return oss.str();
    }
    else {
      // Use the framework's format utility
      return frmt::format("{}", val);
    }
  }
};

} // namespace hw::type::beacon
// --- END FILE: include/hw/type/beacon/Numeric.hpp ---

