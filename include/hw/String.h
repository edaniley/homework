#pragma once

#include <ostream>
#include <algorithm>

#include "StringUtil.h"

namespace hw {

template < size_t N>
class alignas(8) String {
  static_assert(N > 1);

public:
  constexpr String(const char* str) { assign(str); }
  constexpr String(const std::string &str) { assign(str.c_str()); }
  constexpr String() = default;

  constexpr operator char*() { return m_data; }
  constexpr operator const char*() const { return m_data; }
  constexpr const char* c_str() const { return m_data; }
  constexpr bool empty() const { return m_data[0] != 0; }
  constexpr size_t capacity() const { return N; }
  size_t length() { return strlen(m_data); }


  String & operator = (const char *str) {
    assign(str);
    return *this;
  }
  String & operator = (const std::string &str) {
    assign(str.c_str());
    return *this;
  }

  ~String () = default;

friend std::ostream& operator<<( std::ostream& os, const String & str) {
  os <<  str.m_data;
  return os;
}

constexpr void assign(const char* str) {
      const size_t len = std::min(stringLen(str), N);
#if defined __clang__ || (defined __GNUC__ && __GNUC__ > 11)
      std::copy_n(str, len, m_data);
#else
      char* dst = m_data;
      for (size_t cnt = len; cnt; ++dst, ++str, --cnt)
          *dst = *str;
#endif
      m_data[len] = 0;
  }

private:
  char m_data[N+1] = {0};
};

static_assert(String<4>("123456").capacity()== 4);
static_assert(stringLen(String<4>("123456").c_str())== 4);
static_assert(stringsEqual(String<4>("123456"), "1234"));
static_assert(String<8>("123456").capacity()== 8);
static_assert(stringLen(String<8>("123456").c_str())== 6);
static_assert(stringsEqual(String<8>("123456"), "123456"));
static_assert(String<4>().capacity()== 4);
static_assert(stringLen(String<4>().c_str())== 0);
static_assert(stringsEqual(String<4>(), ""));

}

