#pragma once
/******************************************************************************
 * MIT License
 *
 * Copyright (c) 2022 Quirijn Bouts
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/

#ifndef BOQ_METAPROGRAMMING_H
#define BOQ_METAPROGRAMMING_H

#include <cassert>
#include <list>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

//namespace bits_of_q {
namespace mp {

  template <typename T>
  struct has_type {
    using type = T;
  };

  ///////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////// IF_ /////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////

  template <bool condition, typename THEN, typename ELSE>
  struct if_;

  template <typename THEN, typename ELSE>
  struct if_<true, THEN, ELSE> : has_type<THEN> {};

  template <typename THEN, typename ELSE>
  struct if_<false, THEN, ELSE> : has_type<ELSE> {};

  static_assert(std::is_same_v<typename if_<(10 > 5), int, bool>::type, int>);
  static_assert(std::is_same_v<typename if_<(10 < 5), int, bool>::type, bool>);

  ///////////////////////////////////////////////////////////////////////////////
  /////////////////////////// TYPE LIST MANIPULATION ////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////


  template <typename... TYPE_LIST>
  struct type_list {
    using VARIANT = std::variant<TYPE_LIST...>;
    using TUPLE   = std::tuple<TYPE_LIST...>;
  };

  template<typename T> struct is_type_list : std::false_type {};
  template<typename... Ts> struct is_type_list<type_list<Ts...>> : std::true_type {};

  static_assert(is_type_list<type_list<int>>::value);
  static_assert(!is_type_list<int>::value);

  /////////////////////////////////// EMTPY /////////////////////////////////////

  template <typename LIST>
  struct empty : std::false_type {};

  template <template <typename...> class LIST>
  struct empty<LIST<>> : std::true_type {};

  template <typename LIST>
  static constexpr bool empty_v = empty<LIST>::value;

  static_assert(empty_v<type_list<>>);
  static_assert(empty_v<type_list<int, bool>> == false);

  /////////////////////////////////// FRONT /////////////////////////////////////

  template <typename LIST>
  struct front;

  template <template <typename...> class LIST, typename T0, typename... T1toN>
  struct front<LIST<T0, T1toN...>> : has_type<T0> {};

  template <typename LIST>
  using front_t = typename front<LIST>::type;

  static_assert(std::is_same_v<front_t<type_list<int, bool, float>>, int>);

  ///////////////////////////////// POP_FRONT ///////////////////////////////////

  template <typename LIST>
  struct pop_front;

  template <template <typename...> class LIST, typename T0, typename... T1toN>
  struct pop_front<LIST<T0, T1toN...>> : has_type<LIST<T1toN...>> {};

  template <typename LIST>
  using pop_front_t = typename pop_front<LIST>::type;

  static_assert(std::is_same_v<pop_front_t<type_list<int, bool, float>>, type_list<bool, float>>);

  /////////////////////////////////// BACK //////////////////////////////////////

  template <typename LIST>
  struct back : has_type<typename back<pop_front_t<LIST>>::type> {};

  template <template <typename...> class LIST, typename T0>
  struct back<LIST<T0>> : has_type<T0> {};

  template <typename LIST>
  using back_t = typename back<LIST>::type;

  static_assert(std::is_same_v<back_t<type_list<int, bool, float>>, float>);
  static_assert(std::is_same_v<back_t<type_list<int, bool>>, bool>);

  ///////////////////////////////// PUSH_BACK ///////////////////////////////////

  template <typename LIST, typename T>
  struct push_back;

  template <template <typename...> class LIST, typename... T0toN, typename T>
  struct push_back<LIST<T0toN...>, T> : has_type<LIST<T0toN..., T>> {};

  template <typename LIST, typename T>
  using push_back_t = typename push_back<LIST, T>::type;

  static_assert(std::is_same_v<push_back_t<type_list<>, int>, type_list<int>>);
  static_assert(std::is_same_v<push_back_t<type_list<int, bool>, float>, type_list<int, bool, float>>);

  ////////////////////////////////// POP_BACK ///////////////////////////////////

  template <typename FROM_LIST, typename TO_LIST>
  struct make_same_container;

  template <template <typename...> class LIST,
    typename... ELEMS,
    template <typename...>
  class TO_LIST,
    typename... ELEMS2>
  struct make_same_container<LIST<ELEMS...>, TO_LIST<ELEMS2...>> : has_type<TO_LIST<ELEMS...>> {};

  template <typename FROM_LIST, typename TO_LIST>
  using make_same_container_t = typename make_same_container<FROM_LIST, TO_LIST>::type;

  template <typename LIST, typename RET_LIST = make_same_container_t<type_list<>, LIST>>
  struct pop_back;

  template <template <typename...> class LIST, typename T0, typename RET_LIST>
  struct pop_back<LIST<T0>, RET_LIST> : has_type<RET_LIST> {};

  template <template <typename...> class LIST, typename T0, typename T1, typename... T2toN, typename RET_LIST>
  struct pop_back<LIST<T0, T1, T2toN...>, RET_LIST> : pop_back<LIST<T1, T2toN...>, push_back_t<RET_LIST, T0>> {};

  template <typename LIST>
  using pop_back_t = typename pop_back<LIST>::type;

  static_assert(std::is_same_v<pop_back_t<type_list<int>>, type_list<>>);
  static_assert(std::is_same_v<pop_back_t<type_list<int, bool, float>>, type_list<int, bool>>);
  static_assert(std::is_same_v<pop_back_t<type_list<int, bool>>, type_list<int>>);
  static_assert(std::is_same_v<pop_back_t<std::tuple<int, bool>>, std::tuple<int>>);

  //////////////////////////////////// AT ///////////////////////////////////////

  template <typename LIST, size_t index>
  struct at : has_type<typename at<pop_front_t<LIST>, index - 1>::type> {};

  template <typename LIST>
  struct at<LIST, 0> : has_type<front_t<LIST>> {};

  template <typename LIST, size_t index>
  using at_t = typename at<LIST, index>::type;

  static_assert(std::is_same_v<at_t<type_list<int, bool, float>, 1>, bool>);
  static_assert(std::is_same_v<at_t<type_list<int, bool, float>, 2>, float>);

  //////////////////////////////////// ANY //////////////////////////////////////

  template <template <typename> class PREDICATE, typename LIST>
  struct any;

  template <template <typename> class PREDICATE, template <typename...> class LIST>
  struct any<PREDICATE, LIST<>> : std::false_type {};

  template <template <typename> class PREDICATE, typename LIST>
  struct any : if_<  // if predicate matches first type
    PREDICATE<front_t<LIST>>::value,
    // then
    std::true_type,
    // else
    typename any<PREDICATE, pop_front_t<LIST>>::type>::type {};

  template <template <typename> class PREDICATE, typename LIST>
  static constexpr bool any_v = any<PREDICATE, LIST>::value;

  static_assert(any_v<std::is_integral, type_list<int, double, std::string>>);
  static_assert(any_v<std::is_integral, type_list<std::string, double, int>>);
  static_assert(!any_v<std::is_integral, type_list<std::string, double, float>>);

  /////////////////////////////// CONTAINS_TYPE /////////////////////////////////

  template <typename T>
  struct same_as_pred {
    template <typename U>
    struct predicate : std::is_same<T, U> {};
  };

  template <typename SEARCH, typename LIST>
  static constexpr bool contains_type_v = any<same_as_pred<SEARCH>::template predicate, LIST>::value;

  static_assert(contains_type_v<int, type_list<int, bool, float>>);
  static_assert(contains_type_v<float, type_list<int, bool, float>>);
  static_assert(contains_type_v<double, type_list<int, bool, float>> == false);

  /////////////////////////////////// NOT ///////////////////////////////////////

  template <template <typename...> class PRED>
  struct not_ {
    template <typename... Ts>
    using type = std::integral_constant<bool, !PRED<Ts...>::value>;
  };

  ///////////////////////////////////////////////////////////////////////////////

  template <int FIRST, int LAST, typename LAMBDA>
  constexpr void static_for(const LAMBDA& f) {
    if constexpr (FIRST < LAST) {
      f(std::integral_constant<int, FIRST>{});
      static_for<FIRST + 1, LAST>(f);
    }
  }


  ///////////////////////////////////////////////////////////////////////////////
  // added by ED
  ///////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////// SIZE_OF /////////////////////////////////////
  template <typename LIST>
  struct size_of {
    template <typename TList, size_t I>
    static constexpr size_t list_size() {
      if constexpr (empty_v<TList>)
        return I;
      else return list_size < pop_front_t<TList>, I + 1>();
    }
    static constexpr size_t value = list_size<LIST, 0>();
  };

  template <typename LIST>
  static constexpr size_t size_of_v = size_of<LIST>::value;

  static_assert(size_of_v< type_list<int, int, int>> == 3);
  static_assert(size_of_v< type_list<>> == 0);

  ////////////////////////////// FIND INDEX /////////////////////////////////////
  template <typename TYPE, typename LIST>
  struct index {
    static_assert(mp::contains_type_v<TYPE, LIST>);
    static constexpr size_t SIZE = size_of_v<LIST>;

    template <size_t I>
    static constexpr size_t find_index() {
      if constexpr (I < SIZE) {
        using T = at_t<LIST, I>;
        if constexpr (std::is_same_v< TYPE, T>)
          return I;
        else
          return find_index<I+1>();
      }
      return (size_t)-1;
    }
    static constexpr size_t value = find_index<0>();

  };

  template <typename TYPE, typename LIST>
  static constexpr size_t index_v = index<TYPE, LIST>::value;

  static_assert(index_v<int, type_list<double, double, int, double> > == 2);
  static_assert(index_v<char, type_list<char, double, int, double> > == 0);
  //static_assert(index_v<char *, type_list<char, double, int, double> > == (size_t)-1);

  /////////////////////////////// FIND TYPE /////////////////////////////////////
//  template<typename LIST, size_t I = 0>
//  auto find_type(size_t idx) {
//    static constexpr size_t SIZE = size_of_v<LIST>;
//    if constexpr (SIZE >0) {
//    if (idx == I ) {
//      using T = front<LIST>;
//      return (T*)0;
//    }
//    return find_type<pop_front_t<LIST>, I+1>(idx);
//    }
//  }
//
//  template<type_list<>, size_t I>
//  auto find_type(size_t idx) {
//      return (void *)0;
//  }
  //////////////////////////////  IS_UNIQUE /////////////////////////////////////
  template <typename LIST>
  struct is_unique {
    using HEAD = front_t<LIST>;
    using REST = pop_front_t<LIST>;
    static constexpr bool value = contains_type_v< HEAD, REST> ? false :
        is_unique<REST>::value;
  };

  template <>
  struct is_unique<type_list<>> {
    static constexpr bool value = true;
  };

  template <typename LIST>
  static constexpr bool is_unique_v = is_unique<LIST>::value;

  static_assert(is_unique_v<type_list<>>);
  static_assert(is_unique_v<type_list<int, double>>);
  static_assert(false == is_unique_v<type_list<int, int>>);

  ////////////////////////////// MAKE_UNIQUE ////////////////////////////////////
    template <typename LIST>
    struct make_unique {

      template <typename DLIST, typename SLIST>
      struct filter_ {
        using type = filter_ <
                std::conditional_t<
                    contains_type_v<front_t<SLIST>, DLIST>,
                    DLIST,
                    push_back_t< DLIST, front_t<SLIST>>
                >,
                pop_front_t<SLIST>
        >::type;
      };

      template <typename DLIST>
      struct filter_<DLIST, type_list<>>  {
        using type = DLIST;
      };

      using type = decltype(filter_<type_list<>, LIST>())::type;
    };

    template <typename LIST>
    using make_unique_t = typename make_unique<LIST>::type;


    static_assert(std::is_same_v<
        make_unique_t< type_list<char, int, int*, char, int , int>>,
        type_list<char, int, int*>>);


  ////////////////////////////////   ADD    /////////////////////////////////////
  template <typename DLIST, typename SLIST>
  struct add {
    using new_type = front_t<SLIST>;
    using new_list = push_back_t< DLIST, new_type>;
    using rem_list = pop_front_t<SLIST>;
    using type = add<new_list, rem_list>::type;
  };

  template <typename DLIST>
  struct add<DLIST, type_list<>> {
    using type = DLIST;
  };

  template <typename DLIST, typename SLIST>
  using add_t = typename add<DLIST, SLIST>::type;

  static_assert(std::is_same_v <
                add_t<type_list<int, bool>, type_list<int, bool> >,
                type_list<int, bool, int, bool> >);
  static_assert(std::is_same_v <
                add_t<type_list<>, type_list<int, bool> >,
                type_list<int, bool> >);
  static_assert(std::is_same_v <
                add_t<type_list<int, bool>, type_list<> >,
                type_list<int, bool> >);
  static_assert(std::is_same_v <
                add_t<type_list<>, type_list<> >,
                type_list<> >);

  //////////////////////////////// COMBINE //////////////////////////////////////
  template <typename ...>
  struct combine;

  template<typename FIRST, typename SECOND, typename ... REST>
  struct combine<FIRST, SECOND, REST ...> {
    using type = combine<add_t<FIRST, SECOND>, REST ...>::type;
  };

  template <typename FIRST, typename SECOND>
  struct combine <FIRST, SECOND> {
    using type = add_t<FIRST, SECOND>;
  };

  template <typename FIRST>
  struct combine<FIRST>{
    using type = FIRST;
  };

  template <typename ... LISTS>
  using combine_t = typename combine<LISTS ...>::type;

  static_assert(std::is_same_v<
      combine_t<
        type_list<int, double>, type_list<char, float>, type_list<char*>
      >,
      type_list<int, double, char, float, char*>>);
  static_assert(std::is_same_v<
      combine_t<type_list<int, double>>,
      type_list<int, double>>);
  static_assert(std::is_same_v<combine_t<type_list<>>, type_list<>>);

  /////////////////////////////// TRANSFORM /////////////////////////////////////
  template <typename LIST, template < typename ...> class F>
  struct transform {

    template <typename DLIST, typename SLIST>
    struct transform_ {
      using new_type  = F<front_t<SLIST>>::type;
      using new_dlist = push_back_t<DLIST, new_type>;
      using rem_slist = pop_front_t<SLIST>;
      using type      = transform_<new_dlist, rem_slist>::type;
    };

    template <typename DLIST>
    struct transform_<DLIST, type_list<>>  {
      using type = DLIST;
    };

    using type = decltype(transform_<type_list<>, LIST>())::type;
  };

  template <typename LIST, template < typename ...> class F>
  using transform_t = typename transform<LIST, F>::type;


  static_assert(std::is_same_v<
      transform_t< type_list<char, int, int*>, std::add_pointer>,
      type_list<char*, int*, int**>>);

  ///////////////////////////////// FILTER///////////////////////////////////////
    template <typename LIST, template < typename ...> class F>
    struct filter {

      template <typename DLIST, typename SLIST>
      struct filter_ {
        using type = filter_ <
                std::conditional_t<
                    F<front_t<SLIST>>::value,
                    push_back_t< DLIST, front_t<SLIST>>,
                    DLIST
                >,
                pop_front_t<SLIST>
        >::type;
      };

      template <typename DLIST>
      struct filter_<DLIST, type_list<>>  {
        using type = DLIST;
      };

      using type = decltype(filter_<type_list<>, LIST>())::type;
    };

    template <typename LIST, template < typename ...> class F>
    using filter_t = typename filter<LIST, F>::type;


    static_assert(std::is_same_v<
        filter_t< type_list<char, int, int*>, std::is_pointer>,
        type_list<int*>>);

    ///////////////////////////// MERGE TYPE_LISTS ////////////////////////////////
    template<typename LIST1, typename LIST2>
    struct merge {
      static_assert(size_of_v<LIST1> == size_of_v<LIST2>);
      static_assert(size_of_v<LIST1>);

      template<typename DLIST, typename LLIST, typename RLIST>
      struct merge_heads {
        using ltype = std::conditional_t< is_type_list<front_t<LLIST>>::value,
          front_t<LLIST> , type_list<front_t<LLIST>>>;
        using rtype = std::conditional_t< is_type_list<front_t<RLIST>>::value,
          front_t<RLIST> , type_list<front_t<RLIST>>>;
        using new_type = add_t<ltype, rtype>;
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
    using merge_t = typename merge<LIST1, LIST2>::type;

    static_assert(std::is_same_v<
        merge_t< type_list<int, short >,type_list<char*, double*> >,
        type_list<type_list<int, char*>, type_list<short, double*>>
      >);
    static_assert(std::is_same_v<
        merge_t< type_list<int, short >,  type_list<type_list<char, char*>, type_list<double, double*>> >,
        type_list<type_list<int, char, char*>, type_list<short, double, double*>>
      >);


  //////////////////////////////// FOR_EACH /////////////////////////////////////
  // usage examples
  template <typename LIST, size_t I = 0>
  void for_each(auto && cb) {
    if constexpr (empty_v<LIST> == false && I < size_of_v<LIST>) {
      using TType = at_t<LIST, I>;
      cb(I, (TType*)nullptr);
      for_each<LIST, I + 1>(cb);
    }
  };

  template <typename LIST>
  struct for_each2 {
    template <size_t I>
    void for_each_(auto & cb) {
      if constexpr (empty_v<LIST> == false && I < size_of_v<LIST>) {
        using TType = at_t<LIST, I>;
        cb(I, (TType*)nullptr);
        for_each_<I + 1>(cb);
      }
    }
    void operator() (auto & cb) {
      for_each_<0>(cb);
    }
  };
//  auto cb =[](size_t i, auto* ptype) {
//    using TType = std::remove_pointer_t<decltype(ptype)>;
//    cout << "Index:" << i << " Type: " << typeid(TType).name() << endl;
//  };
//
//  mp::for_each<LIST>(cb);
//  mp::for_each2<LIST>()(cb);

}

#endif  // BOQ_METAPROGRAMMING_H
