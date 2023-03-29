#pragma once

#include <iostream>

namespace hw {
template <size_t N> class String;
}

namespace std {
template <size_t N> std::ostream& operator<<( std::ostream&, const hw::String<N>& );
}

namespace hw {

template < size_t N>
class alignas(8) String {
  static constexpr size_t capacity = N;
  static_assert(capacity > 1);

public:
  String() {
    memset(m_data, 0, sizeof(m_data));
  }

  String (const String &other) {
    memcpy(m_data, &other, sizeof(m_data));
  }

  String(const char *str) {
    copy(str);
  }

  String(const std::string &str) {
    copy(str.c_str());
  }

  String & operator = (const String &other) {
    memcpy(m_data, &other, sizeof(m_data));
    return *this;
  }

  String & operator = (const char *str) {
    copy(str);
    return *this;
  }

  String & operator = (const std::string &str) {
    copy(str.c_str());
    return *this;
  }

  operator const char *() const {
    return m_data;
  }

  ~String () = default;

private:
  char m_data[capacity];
  void copy(const char *str) {
    const size_t len = std::min(::strlen(str), sizeof(m_data)-1);
    memcpy(m_data, str, len);
    m_data[len] = 0;
  }

  friend std::ostream& std:: operator<< <N>( std::ostream&, const String<N>& );
};

}

namespace std {
template <size_t N>
std::ostream & operator<< (std::ostream &os, const hw::String<N>& str) {
  os << std::string((const char *)str);
  return os;
}
}
