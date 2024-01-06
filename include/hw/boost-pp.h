#pragma once

#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/seq/pop_front.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/variadic/elem.hpp>
#include <boost/preprocessor/cat.hpp>

// Such technique is used at:
//   http://www.boost.org/doc/libs/1_56_0/boost/fusion/adapted/struct/define_struct.hpp
#define AUXILIARY_0(...) ((__VA_ARGS__)) AUXILIARY_1
#define AUXILIARY_1(...) ((__VA_ARGS__)) AUXILIARY_0
#define AUXILIARY_0_END
#define AUXILIARY_1_END

#define REMOVE_PARENTHESES(...) __VA_ARGS__

#define COMMA_SEPARATED(r, data, i, elem) \
    BOOST_PP_COMMA_IF(i) BOOST_PP_VARIADIC_ELEM(data, REMOVE_PARENTHESES elem) \
/**/

#define ZIPPED_TO_SEQ(zipped) \
    BOOST_PP_SEQ_POP_FRONT(BOOST_PP_CAT(AUXILIARY_0(0)zipped,_END)) \
/**/

#define FOR_EACH_ZIPPED_I(macro, data, zipped) \
    BOOST_PP_SEQ_FOR_EACH_I(macro, data, ZIPPED_TO_SEQ(zipped)) \
/**/

#define UNZIP(i, zipped) FOR_EACH_ZIPPED_I(COMMA_SEPARATED, i, zipped)
//https://stackoverflow.com/questions/26475453/how-to-use-boostpreprocessor-to-unzip-a-sequence



