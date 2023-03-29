#pragma once

#include <concepts>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <variant>
#include <tuple>
#include <array>
#include <cstring>

#include "MP.h"
#include "Struct-pp.h"

namespace hw {
  // Field
  template<size_t N>
  struct TName {
    constexpr TName(const char (&str)[N]) {
      std::copy_n(str, N, value);
    }
    char value[N];
  };

  template <typename F, TName tag>
  struct Field {
    using value_type = F;
    static constexpr std::string_view NAME = {tag.value, sizeof(tag.value)};
    F m_val;

    template<typename T>
    Field(const T &val) : m_val(val) {}

    F &       operator()()       {return m_val;}
    const F & operator()() const {return m_val;}
    static std::string Name() {
      const char *data = NAME.data();
      return std::string(data, strlen(data));
    }
    static std::string TypeToString() {
      std::ostringstream os;
      os << hw::DemangleTypeName(typeid(F).name())<< "," << Name();
      return os.str();
    }
  };

  template <typename S, TName tag>
  struct NamedStruct {
    using type = S;
    static constexpr std::string_view NAME = {tag.value, sizeof(tag.value)};
    static std::string Name() {
      const char *data = NAME.data();
      return std::string(data, strlen(data));
    }
    static std::string TypeToString() {
      std::ostringstream os;
      os << hw::DemangleTypeName(typeid(S).name())<< "," << Name();
      return os.str();
    }
  };

  // Copy structs
  template <typename TSrcStruct, typename TDstStruct>
  struct CopyStruct {
    template <size_t S, size_t D >
    static void CopyField_(const TSrcStruct& src, TDstStruct &dst) {
      if constexpr (S < TSrcStruct::FIELD_CNT) {
        using TSrcField = mp::at_t<typename TSrcStruct::FIELD_LIST, S>;
        using TDstField = mp::at_t<typename TDstStruct::FIELD_LIST, D>;
        if constexpr (std::is_same_v<TSrcField, TDstField>) {
          dst.template GetField<TDstField>() = src.template GetField<TSrcField>();
        }
        else {
          CopyField_<S + 1, D>(src, dst);
        }
      }
    }

    template <size_t S, size_t D>
    static void Copy_(const TSrcStruct & src, TDstStruct & dst) {
      if constexpr (D < TDstStruct::FIELD_CNT) {
        CopyField_< 0, D   >(src, dst);
        Copy_     < 0, D+1 >(src, dst);
      }
    }

    void operator() (const TSrcStruct& src, TDstStruct& dst) {
      Copy_<0, 0>(src, dst);
    }
  };

  // Copy lists of structs: main call from CopyFrom/CopyTo
  template <typename TSrcStruct, typename TDstStruct>
  struct CopyStructList {
    template <size_t S, size_t D >
    static void CopyStruct_(const TSrcStruct& src, TDstStruct &dst) {
      if constexpr (S < TSrcStruct::STRUCT_CNT) {
        using SRC_STRUCT_TYPE = typename mp::at_t<typename TSrcStruct::STRUCT_LIST, S>;
        using DST_STRUCT_TYPE = typename mp::at_t<typename TDstStruct::STRUCT_LIST, D>;
        CopyStruct<SRC_STRUCT_TYPE, DST_STRUCT_TYPE>()(
            static_cast<const SRC_STRUCT_TYPE &>(src),
            static_cast<DST_STRUCT_TYPE &>(dst)
        );
        CopyStruct_<S + 1, D>(src, dst);
      }
    }

    template <size_t S, size_t D>
    static void Copy_(const TSrcStruct & src, TDstStruct & dst) {
      if constexpr (D < TDstStruct::STRUCT_CNT) {
        CopyStruct_< 0, D   >(src, dst);
        Copy_     < 0, D+1 >(src, dst);
      }
    }

    void operator() (const TSrcStruct& src, TDstStruct& dst) {
      Copy_<0, 0>(src, dst);
    }

  };

  template <typename TStruct>
  struct PrintStruct {
    template <typename S, size_t I>
    static void PrintField_(std::ostream &os, const S& s) {
      if constexpr (I < S::FIELD_CNT) {
        using TField = mp::at_t<typename S::FIELD_LIST, I>;
        os << ';' << TField::Name() << ':' << s.template GetField<TField>();
        PrintField_<S, I + 1>(os, s);
      }
    }
    template <size_t I>
    static void Print_(std::ostream &os, const TStruct& s) {
      if constexpr (I < TStruct::STRUCT_CNT) {
        using S = mp::at_t<typename TStruct::STRUCT_LIST, I>;
        PrintField_<S, 0>(os, static_cast<const S&>(s));
        Print_<I + 1>(os, s);
      }
    }
  };

  template <typename TStruct>
  std::string StructToString (const TStruct &s){
    std::ostringstream os;
    PrintStruct<TStruct>::template Print_<0>(os, s);
    return os.str().substr(1);
  };

  // STRUCT BASE
  template <typename TStruct>
  struct StructBase {
    template<typename TField>
    TField::value_type & GetField() {
      return static_cast<TField &>( static_cast<TStruct &>(*this) ).m_val;
    }
    template<typename TField>
    const TField::value_type & GetField() const {
      return static_cast<const TField &>(static_cast<const TStruct &>(*this)).m_val;
    }

    template<typename TOther>
    void CopyTo(TOther &other) const {
      CopyStructList<TStruct, TOther>()(static_cast<const TStruct &>(*this), other);
    }
    template<typename TOther>
    void CopyFrom(const TOther &other) {
      CopyStructList<TOther, TStruct>()(other, static_cast<TStruct &>(*this));
    }

    std::string ToString() const {
      return StructToString<TStruct>(static_cast<const TStruct &>(*this));
    }
  };

  template<typename TStruct>
  std::string TypeListToString(const char *namesep = ":", const char *fldsep = ";") {
    std::ostringstream os;
    auto cb =[&os, namesep, fldsep ](size_t index, auto* ptype) {
      using TFieldType = std::remove_pointer_t<decltype(ptype)>;
      os << TFieldType::Name() << namesep
           << hw::DemangleTypeName(typeid(typename TFieldType::value_type).name());
      if (index < (TStruct::FIELD_CNT-1)) os << fldsep;
    };
    mp::for_each<typename TStruct::FIELD_LIST>(cb);
    return os.str();
  }

} // namespace hw
