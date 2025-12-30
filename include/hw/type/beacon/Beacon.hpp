#pragma once

#include <cstdint>
#include <chrono>

namespace hw::type::beacon {

using Byte = int8_t;
using UByte = uint8_t;
using Char = char;
using Short = int16_t;
using Int = int32_t;
using Long = int64_t;
#ifdef __SIZEOF_INT128__
using LLong = __int128_t;
#else
#include <boost/multiprecision/cpp_int.hpp>
using LLong = boost::multiprecision::int128_t;
#endif
using Price = Long;
using Double = double;
using Time = std::chrono:: system_clock:: time_point;
struct OpaqueTiny {};
struct OpaqueSmall {};

}
