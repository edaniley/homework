// --- START FILE: include/hw/type/beacon/TypeTraits.hpp ---
#pragma once

#include <hw/type/TypeList.hpp>

namespace hw::type::beacon {

namespace trait {
  struct Unknown {};
  struct Numeric {};
  struct Enum {};
  struct VarString {};
  struct PaddedString {};
  struct Opaque {};
  struct Optional {};
  struct Accessor {};
  struct Message {};
}

/**
 * QueryTrait: Safely extracts the nested ::type_trait from a type.
 * Defaults to trait::Unknown if no trait is defined.
 */
template<typename T, typename = void>
struct QueryTrait {
  using type = trait::Unknown;
};

template<typename T>
struct QueryTrait<T, std::void_t<typename T::type_trait>> {
  using type = typename T::type_trait;
};

// Convenience alias for cleaner meta-programming
template<typename T>
using QueryTrait_t = typename QueryTrait<T>::type;

using BeaconFieldTypeList = type_list<
  trait::Numeric,
  trait::Enum,
  trait::VarString,
  trait::PaddedString,
  trait::Opaque,
  trait::Optional,
  trait::Accessor,
  trait::Message
>;

template<typename Type>
using IsBeaconFieldType = mp_contains<BeaconFieldTypeList, QueryTrait_t<Type>>;

template<typename Type>
concept BeaconFieldType = IsBeaconFieldType<Type>::value;

using AccessorFieldTypeList = type_list<
  trait::Numeric,
  trait::PaddedString,
  trait::Enum,
  trait::Optional,
  trait::Accessor,
  trait::Message
>;

template<typename Type>
using IsAccessorFieldType = mp_contains<AccessorFieldTypeList, QueryTrait_t<Type>>;

template<typename Type>
concept AccessorFieldType = IsAccessorFieldType<Type>::value;

}
// --- END FILE: include/hw/type/beacon/TypeTraits.hpp ---