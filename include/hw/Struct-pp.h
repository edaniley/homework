#pragma once

#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/punctuation/remove_parens.hpp>
#include <boost/preprocessor/repetition.hpp>
#include <boost/preprocessor/arithmetic/sub.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/iteration/local.hpp>
#include <boost/preprocessor/repetition/enum_params.hpp>
#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/tuple.hpp>
#include <boost/preprocessor/list.hpp>

#include "boost-pp.h"

#define HW_MAKE_List(n, d, name) BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, 1)) name

#define HW_MAKE_TypeList(n, data, tup)                                    \
    BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, 1))                                 \
    hw::Field<                                                            \
      BOOST_PP_TUPLE_ELEM(0, tup),                                        \
      BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(1, tup))                     \
    >

#define HW_MAKE_DefaultValues(n, data, tup)                               \
  BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, 1))                                   \
  BOOST_PP_CAT(m_, BOOST_PP_TUPLE_ELEM(1, tup)) (                         \
  BOOST_PP_TUPLE_ELEM(2, tup) )

#define HW_MAKE_CtorArguments(n, data, tup)                               \
  BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, 1))                                   \
  BOOST_PP_TUPLE_ELEM(0, tup)                                             \
  BOOST_PP_CAT(a_, BOOST_PP_TUPLE_ELEM(1, tup))

#define HW_MAKE_CtorMembers(n, data, tup)                                 \
  BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, 1))                                   \
  BOOST_PP_CAT(m_, BOOST_PP_TUPLE_ELEM(1, tup)) (                         \
  BOOST_PP_CAT(a_, BOOST_PP_TUPLE_ELEM(1, tup)) )

#define HW_MAKE_CombinedTypeList(n, list_type, struct_name)               \
  BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, 1))                                   \
  typename struct_name:: list_type

#define HW_MAKE_TypeMembers(n, data, tup)                                       \
  hw::Field<                                                                    \
    BOOST_PP_TUPLE_ELEM(0, tup),                                                \
    BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(1, tup))                             \
  > BOOST_PP_CAT(m_, BOOST_PP_TUPLE_ELEM(1, tup));                              \
  operator hw::Field<                                                           \
    BOOST_PP_TUPLE_ELEM(0, tup),                                                \
    BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(1, tup))                             \
  >  &() { return BOOST_PP_CAT(m_, BOOST_PP_TUPLE_ELEM(1, tup)); }              \
  operator const hw::Field<                                                     \
    BOOST_PP_TUPLE_ELEM(0, tup),                                                \
      BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(1, tup))                           \
  >  &() const { return BOOST_PP_CAT(m_, BOOST_PP_TUPLE_ELEM(1, tup)); }        \
  BOOST_PP_TUPLE_ELEM(0, tup) & BOOST_PP_TUPLE_ELEM(1, tup) () {                \
    return BOOST_PP_CAT(m_, BOOST_PP_TUPLE_ELEM(1, tup)).m_val; }               \
  const BOOST_PP_TUPLE_ELEM(0, tup) & BOOST_PP_TUPLE_ELEM(1, tup) () const {    \
    return BOOST_PP_CAT(m_, BOOST_PP_TUPLE_ELEM(1, tup)).m_val; }


#define HW_DEF_STRUCT(STRUCT_NAME, MEMBER_SEQ )                                 \
struct STRUCT_NAME : hw::StructBase<STRUCT_NAME> {                              \
  using STRUCT_LIST = mp::type_list<STRUCT_NAME>;                               \
  using FIELD_LIST = mp::type_list<                                             \
      BOOST_PP_SEQ_FOR_EACH(HW_MAKE_TypeList, ~, ZIPPED_TO_SEQ(MEMBER_SEQ)) >;  \
  static const size_t STRUCT_CNT = 1;                                           \
  static const size_t FIELD_CNT = BOOST_PP_SEQ_SIZE(ZIPPED_TO_SEQ(MEMBER_SEQ)); \
  static_assert(mp::is_unique_v<FIELD_LIST>);                                   \
  static_assert(mp::is_unique_v<STRUCT_LIST>);                                  \
  STRUCT_NAME() :                                                               \
  BOOST_PP_SEQ_FOR_EACH(HW_MAKE_DefaultValues, ~, ZIPPED_TO_SEQ(MEMBER_SEQ)) {} \
  STRUCT_NAME(                                                                  \
  BOOST_PP_SEQ_FOR_EACH(HW_MAKE_CtorArguments, ~, ZIPPED_TO_SEQ(MEMBER_SEQ)) )  \
  : BOOST_PP_SEQ_FOR_EACH(HW_MAKE_CtorMembers, ~, ZIPPED_TO_SEQ(MEMBER_SEQ)) {} \
  BOOST_PP_SEQ_FOR_EACH(HW_MAKE_TypeMembers, ~, ZIPPED_TO_SEQ(MEMBER_SEQ))      \
}

#define HW_CAT_STRUCT(tup)  \
struct BOOST_PP_TUPLE_ELEM(BOOST_PP_TUPLE_SIZE(tup), 0, tup) :                  \
  BOOST_PP_SEQ_FOR_EACH(HW_MAKE_List, ~,                                        \
  BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_TUPLE_POP_FRONT(tup))) {                       \
  using TSelf = BOOST_PP_TUPLE_ELEM(BOOST_PP_TUPLE_SIZE(tup), 0, tup);          \
  using STRUCT_LIST = mp::combine_t<                                            \
    BOOST_PP_SEQ_FOR_EACH(HW_MAKE_CombinedTypeList, STRUCT_LIST,                \
    BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_TUPLE_POP_FRONT(tup))) >;                    \
  using FIELD_LIST = mp::combine_t<                                             \
    BOOST_PP_SEQ_FOR_EACH(HW_MAKE_CombinedTypeList, FIELD_LIST,                 \
    BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_TUPLE_POP_FRONT(tup))) >;                    \
  static const size_t STRUCT_CNT = mp::size_of_v< STRUCT_LIST >;                \
  static const size_t FIELD_CNT = mp::size_of_v< FIELD_LIST >;                  \
  static_assert(mp::is_unique_v<FIELD_LIST>);                                   \
  static_assert(mp::is_unique_v<STRUCT_LIST>);                                  \
  template<typename TOther>                                                     \
  void CopyTo(TOther &other) const { hw::CopyStructList<TSelf,                  \
    TOther>()(static_cast<const TSelf &>(*this), other);}                       \
  template<typename TOther>                                                     \
  void CopyFrom(const TOther &other) {                                          \
    hw::CopyStructList<TOther, TSelf>()(other, static_cast<TSelf &>(*this));}   \
  std::string ToString() const {                                                \
    return hw::StructToString<TSelf>(static_cast<const TSelf &>(*this));}       \
}
