// --- START FILE: include/hw/type/beacon/String.hpp ---
#pragma once

#include <cstring>
#include <string>
#include <string_view>
#include <algorithm>
#include <stdexcept>

#include <hw/type/NameTag.hpp>
#include <hw/type/beacon/TypeTraits.hpp>

namespace hw::type::beacon {

namespace detail {
  /**
   * Constexpr-friendly null-terminator search within a fixed limit.
   */
  constexpr size_t constexpr_strnlen(const char* s, size_t maxlen) {
    for (size_t i = 0; i < maxlen; ++i) {
      if (s[i] == '\0') return i;
    }
    return maxlen;
  }
}

template<NameTag Tag, size_t Size, char Padding>
struct NamedStringType {
  using type_trait = std::conditional_t<Padding == '\0', trait::VarString, trait::PaddedString>;
  using value_type = char[Size];
  static constexpr size_t SIZE = Size;
  static_assert(SIZE > 0, "Size must be greater than zero.");
  static constexpr NameTag name_tag = Tag;

  /**
   * Returns actual string size if null-terminated (\0),
   * otherwise returns the full capacity (SIZE).
   */
  static constexpr size_t size(const std::byte* ptr) {
    if constexpr (Padding == '\0') {
      return detail::constexpr_strnlen(reinterpret_cast<const char*>(ptr), SIZE);
    } else {
      return SIZE;
    }
  }

  static void set(std::byte* ptr, std::string_view sv) {
    size_t len = std::min(SIZE, sv.length());
    std::memcpy(ptr, sv.data(), len);
    if (len < SIZE) {
      std::memset(ptr + len, Padding, SIZE - len);
    }
  }

  template <typename Type>
  requires requires (Type t) { std::to_string(t); }
  static void set(std::byte* ptr, Type&& value) {
    std::string val = std::to_string(std::forward<Type>(value));
    set(ptr, std::string_view(val));
  }

  static std::string_view get(const std::byte* ptr) {
    return {reinterpret_cast<const char*>(ptr), size(ptr)};
  }

  static std::string toString(const std::byte* ptr) {
    return std::string(get(ptr));
  }
};

template<NameTag Tag, size_t Size>
using NamedFixedStringType = NamedStringType<Tag, Size, ' '>;

template<NameTag Tag, size_t Size>
using NamedVariableStringType = NamedStringType<Tag, Size, '\0'>;

} // namespace hw::type::beacon