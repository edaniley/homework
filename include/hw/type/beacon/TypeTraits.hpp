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
struct QueryTypeTrait {
  using type = trait::Unknown;
};

template<typename T>
struct QueryTypeTrait<T, std::void_t<typename T::type_trait>> {
  using type = typename T::type_trait;
};

// Convenience alias for cleaner meta-programming
template<typename T>
using QueryTrait = typename QueryTypeTrait<T>::type;

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
using IsBeaconFieldType = mp_contains<BeaconFieldTypeList, QueryTrait<Type>>;

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
using IsAccessorFieldType = mp_contains<AccessorFieldTypeList, QueryTrait<Type>>;

template<typename Type>
concept AccessorFieldType = IsAccessorFieldType<Type>::value;

using OptionalFieldTypeList = type_list<
  trait::Numeric,
  trait::Enum,
  trait::VarString,
  trait::PaddedString,
  trait::Opaque
>;

template<typename Type>
using IsOptionalFieldType = mp_contains<OptionalFieldTypeList, QueryTrait<Type>>;

template<typename Type>
concept OptionalFieldType = IsOptionalFieldType<Type>::value;

}
// --- END FILE: include/hw/type/beacon/TypeTraits.hpp ---