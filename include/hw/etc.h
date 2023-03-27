#pragma once

#include <cassert>
#include <iostream>
#include <sstream>
#include <memory>
#ifdef unix
#include <cxxabi.h>
#endif

#define LIKELY(x)    __builtin_expect (!!(x), 1)
#define UNLIKELY(x)  __builtin_expect (!!(x), 0)

namespace hw
{
  inline void Abort(const char *errmsg, const char *errloc) {
    std::cerr << errloc << " [" << errmsg << "]" << std::endl;
  }

#ifdef unix
  inline std::string DemangleTypeName(const char *typeName) {
    int status;
    std::unique_ptr<char, void(*)(void*)> ptr(
      abi::__cxa_demangle(typeName, 0, 0, &status),
      std::free);
    if (!status)
      return std::string(ptr.get());
    return std::string("");
  }
#else
#define DemangleTypeName(x) x
#endif

  template <typename T>
  std::string TypeName() {
    return DemangleTypeName(typeid(T).name());
  }

#define HW_ABORT(MSG) Abort(MSG, __FILE_ ":" __LINE__);

}
