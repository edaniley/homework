#pragma once

#include <memory>

#include "TypeList.h"
#include "Util.h"

namespace mp {

constexpr bool strings_equal(char const* a, char const* b) {
  return *a == *b && (*a == '\0' || strings_equal(a + 1, b + 1));
}

template<typename T>
struct add_unique_ptr {
  using type = std::unique_ptr<T>;
};

template<typename T>
using add_unique_ptr_t = typename add_unique_ptr<T>::type;

//////////////////////////////// std::tuple ///////////////////////////////////
///////////////////////////////// IS_TUPLE ////////////////////////////////////
template<typename T> struct is_tuple : std::false_type {};
template<typename... Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

static_assert(is_tuple<std::tuple<int>>::value);
static_assert(!is_tuple<type_list<int>>::value);
static_assert(!is_tuple<int>::value);

///////////////////////////// MERGE TYPE_LISTS ////////////////////////////////
template<typename LIST1, typename LIST2>
struct tuple_merge {
  static_assert(size_of_v<LIST1> == size_of_v<LIST2>);
  static_assert(size_of_v<LIST1>);

  template<typename DLIST, typename LLIST, typename RLIST>
  struct merge_heads {
    using ltype = std::conditional_t< is_tuple<front_t<LLIST>>::value,
      front_t<LLIST> , std::tuple<front_t<LLIST>>>;
    using rtype = std::conditional_t< is_tuple<front_t<RLIST>>::value,
      front_t<RLIST> , std::tuple<front_t<RLIST>>>;
    using new_type = decltype(std::tuple_cat(ltype{}, rtype{}));
    using new_list = push_back_t<DLIST, new_type>;
    using type = merge_heads< push_back_t<DLIST, new_type>,
                              pop_front_t<LLIST>,
                              pop_front_t<RLIST> >::type;
  };
  template<typename DLIST>
  struct merge_heads<DLIST, type_list<>,type_list<>> {
    using type = DLIST;
  };
  using type = merge_heads<type_list<>, LIST1, LIST2>::type;
};

template<typename LIST1, typename LIST2>
using tuple_merge_t = typename tuple_merge<LIST1, LIST2>::type;


static_assert(std::is_same_v<
    tuple_merge_t< type_list<int, short >,type_list<char*, double*> >,
    type_list<std::tuple<int, char*>, std::tuple<short, double*>>
  >);
static_assert(std::is_same_v<
    tuple_merge_t<
      type_list<int, short >,
      type_list<std::tuple<char, char*>, std::tuple<double, double*>>
    >,
    type_list<std::tuple<int, char, char*>, std::tuple<short, double, double*>>
  >);

////////////////////////////////// FOR_EACH ///////////////////////////////////
template <typename TUPLE, size_t I = 0>
void tuple_for_each (TUPLE &tup, auto&&cb) {
  if constexpr(I < std::tuple_size_v<TUPLE>) {
      cb(std::get<I>(tup));
      tuple_for_each<TUPLE, I+1>(tup, cb);
  }
};


// extract list of types from list of tuples by index
/*
error: invalid use of incomplete type ‘struct std::tuple_element<1, mp::type_list<std::tuple<
template <typename TUP_LIST, size_t INDEX>
struct list_from_2d_list {
  static constexpr size_t SIZE = mp::size_of_v <TUP_LIST>;
  static_assert(SIZE == 0 ||  INDEX < std::tuple_size_v<mp::front_t<TUP_LIST>>);

  template <typename DLIST, typename SLIST, size_t I>
  struct add_ {
    //error: invalid use of incomplete type ‘struct std::tuple_element<1, mp::type_list<std::tuple<
    using new_type = typename std::tuple_element_t<INDEX, typename mp::front_t<SLIST>>;
    using new_list = typename mp::push_back_t<DLIST, new_type>;
    using rem_list = typename mp::pop_front<SLIST>;
    using type = add_<new_list, rem_list, I+1>::type;
  };

  template <typename DLIST, typename SLIST>
  struct add_<DLIST, SLIST, SIZE> {
    using type = DLIST;
  };

  using type = add_<mp::type_list<>, TUP_LIST, 0>::type;
};

template <typename TUP_LIST, size_t INDEX>
using list_from_2d_list_t = typename list_from_2d_list<TUP_LIST, INDEX>::type;

using test_2d_list = mp::type_list<
    std::tuple<int, double>, std::tuple<double, int>, std::tuple<char, char*>
>;
static_assert(std::is_same_v< list_from_2d_list<test_2d_list, 1>::type,
                              mp::type_list<double, int, char*> >);
*/

/////////////////////////////// std::variant //////////////////////////////////

template <typename Tuple>
struct tuple_to_variant;

template <typename... Ts>
struct tuple_to_variant<std::tuple<Ts...>>
{
    using type = std::variant<std::monostate,  Ts  ...>;
};

///////////////////////////// HAS ALTERNATIVE /////////////////////////////////
template <typename VARIANT, typename ALTERNATIVE>
struct has_alternative {
  template <size_t I>
  static constexpr bool check_type () {
    if constexpr (I >= std::variant_size_v<VARIANT>)
      return false;
    else if constexpr ( std::is_same_v< ALTERNATIVE, std::variant_alternative_t<I, VARIANT> >)
      return true;
    else
      return check_type<I+1>();
 }
  static constexpr bool value = check_type<0>();
};

//template <typename VARIANT, typename MEMBER>
//struct has_alternative {
//private:
//  template<typename V, typename M, size_t I = 0>
//  static constexpr size_t index_() {
//    if constexpr (I >= std::variant_size_v<V>) {
//      return std::variant_size_v<V>;
//    }
//    else {
//      if constexpr (std::is_same_v<std::variant_alternative_t<I, V>, M>) {
//        return I;
//      }
//      else {
//        return index_<V, M, I + 1>();
//      }
//    }
//  }
//public:
//  static constexpr bool value = index_<VARIANT, MEMBER>() < std::variant_size_v<VARIANT>;
//};

template <typename V, typename M>
static constexpr bool has_alternative_v = has_alternative<V, M>::value;

static_assert(has_alternative_v<std::variant<int, char*>, int>);
static_assert(!has_alternative_v<std::variant<int, char*>, char>);

//////////////////////////////// std::array ///////////////////////////////////
//////////////////////////////// ARRAY_CAT ////////////////////////////////////
// Concatenate two arrays
template<typename ARRAY1, typename ARRAY2>
struct array_cat {
  static const size_t size = std::tuple_size<ARRAY1>::value + std::tuple_size<ARRAY2>::value;
  using value_type = ARRAY1::value_type;
  using rettype = std::array<value_type, size>;
  constexpr rettype operator ()(const ARRAY1 & arr1, const ARRAY2 & arr2) {
    rettype retval;
    std::copy(arr1.begin(), arr1.end(), retval.begin());
    std::copy(arr2.begin(), arr2.end(), retval.begin() + std::tuple_size<ARRAY1>::value);
    return retval;
  }
};

constexpr std::array arr1{"One","Two"};
constexpr std::array arr2{"99", "100"};
constexpr std::array arr3{"One","Two","99","100"};
constexpr auto arr12 = array_cat<decltype(arr1), decltype(arr2)>()(arr1, arr2);
static_assert(arr12 == arr3);

}
