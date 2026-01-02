// --- START FILE: include/hw/type/beacon/Opaque.hpp ---
#pragma once

#include <cstring>
#include <cstdint>
#include <string>
#include <string_view>
#include <algorithm>
#include <concepts>

#include <hw/utility/Text.hpp>
#include <hw/utility/Format.hpp>
#include <hw/type/beacon/TypeTraits.hpp>
#include <hw/type/beacon/Beacon.hpp>

namespace hw::type::beacon {

template <size_t CNT, bool READONLY = false>
class Opaque {
public:
  using type_trait = trait::Opaque;
  using pointer_type = std::conditional_t<READONLY, const std::byte*, std::byte*>;

  static constexpr size_t MAX_PAYLOAD_SIZE = CNT;
  static constexpr size_t MAX_MEM_SIZE = MAX_PAYLOAD_SIZE + sizeof(Short);

  explicit Opaque(pointer_type ptr) : _ptr(ptr) {
    if constexpr (!READONLY) {
      // Initialize size to 0 on construction for Editors
      setPayloadSize(0);
    }
  }

  // Returns current total size (Prefix + Payload)
  size_t size() const {
    return sizeof(Short) + payloadSize();
  }

  // Returns current payload size from the first 2 bytes
  size_t payloadSize() const {
    Short ps;
    std::memcpy(&ps, _ptr, sizeof(Short));
    return static_cast<size_t>(ps);
  }

  pointer_type data() const { return _ptr; }

  // Pointer to the start of the payload
  pointer_type head() const { return _ptr + sizeof(Short); }

  // Pointer to the first byte after the current payload
  pointer_type tail() const { return _ptr + size(); }

  /**
   * Setters (Requires READONLY == false)
   */

  template <typename Type>
  void append(const Type& value) requires (!READONLY) {
    if (payloadSize() + sizeof(Type) > MAX_PAYLOAD_SIZE) {
      throw std::out_of_range("Opaque: append exceeds MAX_PAYLOAD_SIZE");
    }
    std::memcpy(tail(), &value, sizeof(Type));
    setPayloadSize(payloadSize() + sizeof(Type));
  }

  template <typename Type>
  void pad(size_t count, const Type& value = Type{}) requires (!READONLY) {
    for (size_t i = 0; i < count; ++i) {
      append(value);
    }
  }

  // Sets payload from string without null-terminator
  void set(const char* str) requires (!READONLY) {
    if (!str) return;
    std::string_view sv(str);
    size_t len = std::min(sv.length(), MAX_PAYLOAD_SIZE);
    std::memcpy(head(), sv.data(), len);
    setPayloadSize(len);
  }

  // Parses hex representation (supports 0x prefix and spaces)
  void fromString(std::string_view hex_str) requires (!READONLY) {
    std::string clean = hw::utility::trim(std::string(hex_str));

    // Skip 0x if present
    size_t offset = 0;
    if (clean.size() >= 2 && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) {
      offset = 2;
    }

    std::byte* dst = head();
    size_t bytes_written = 0;

    for (size_t i = offset; i + 1 < clean.size() && bytes_written < MAX_PAYLOAD_SIZE; ) {
      if (std::isspace(static_cast<unsigned char>(clean[i]))) {
        i++;
        continue;
      }

      char hi = clean[i++];
      char lo = clean[i++];
      dst[bytes_written++] = static_cast<std::byte>((hw::utility::toNibble(hi) << 4) | hw::utility::toNibble(lo));
    }
    setPayloadSize(bytes_written);
  }

  /**
   * Observers
   */

  // Returns hex representation without 0x prefix
  std::string toString() const {
    size_t len = payloadSize();
    if (len == 0) return "";

    const uint8_t* src = reinterpret_cast<const uint8_t*>(head());
    std::string res;
    res.reserve(len * 2);

    for (size_t i = 0; i < len; ++i) {
      res += frmt::format("{:02x}", src[i]);
    }
    return res;
  }

private:
  void setPayloadSize(size_t sz) requires (!READONLY) {
    Short ps = static_cast<Short>(sz);
    std::memcpy(_ptr, &ps, sizeof(Short));
  }

  pointer_type const _ptr;
};

// --- NamedOpaqueType ---

template <NameTag Tag, size_t CNT>
struct NamedOpaqueType {
  using type_trait = trait::Opaque;
  using Editor = Opaque<CNT, false>;
  using Viewer = Opaque<CNT, true>;

  static constexpr size_t MAX_PAYLOAD_SIZE = CNT;
  static constexpr size_t MAX_MEM_SIZE = CNT + sizeof(Short);
  static constexpr NameTag name_tag = Tag;
};

} // namespace hw::type::beacon

// --- END FILE: include/hw/type/beacon/Opaque.hpp ---
