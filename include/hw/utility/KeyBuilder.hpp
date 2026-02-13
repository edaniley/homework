#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <tuple>
#include <algorithm>
#include <string_view>
#include <type_traits>
#include <initializer_list>

#include <hw/type/NameTag.hpp>
#include <hw/type/TypeList.hpp>

namespace hw::utility {

// Field Definition Tuple
// Usage: KeyAttribute<"Name", Size, Inserter>
template <type::NameTag Name, size_t Size, typename Inserter>
struct KeyAttribute {
    static constexpr auto name = Name;
    static constexpr size_t size = Size;
    using inserter_type = Inserter;
};

// Helper for Builder Arguments (supports bool or string literal)
template <size_t N>
struct BuilderArg {
    bool is_bool = false;
    bool b_val = false;
    char name[N]{};

    constexpr BuilderArg(bool b) : is_bool(true), b_val(b) {}
    constexpr BuilderArg(const char (&str)[N]) {
        for(size_t i=0; i<N; ++i) name[i] = str[i];
    }
    
    constexpr auto asNameTag() const {
        return type::NameTag<N>(name);
    }
};

// Deduction guides for BuilderArg
template <size_t N> BuilderArg(const char (&)[N]) -> BuilderArg<N>;
BuilderArg(bool) -> BuilderArg<1>;


template <typename T, typename AccessorList>
class KeyBuilder;

// Specialization to unpack the AccessorList
// We specialize on type::type_list
template <typename T, typename... Fields>
class KeyBuilder<T, type::type_list<Fields...>> {
    
    // Helper to find a field by name in the AccessorList
    template <type::NameTag Name, typename Tuple>
    struct find_field;

    template <type::NameTag Name>
    struct find_field<Name, std::tuple<>> {
        using type = void; // Not found
    };

    template <type::NameTag Name, typename Head, typename... Tail>
    struct find_field<Name, std::tuple<Head, Tail...>> {
        using type = std::conditional_t<
            (Head::name.toString() == Name.toString()),
            Head,
            typename find_field<Name, std::tuple<Tail...>>::type
        >;
    };

    // Helper to parse comma separated list
    static constexpr std::string_view trim(std::string_view s) {
        size_t first = s.find_first_not_of(" \t");
        if (std::string_view::npos == first) {
            return s;
        }
        size_t last = s.find_last_not_of(" \t");
        return s.substr(first, (last - first + 1));
    }

    // Helper to detect default_byte member
    template <typename Attr, typename = void>
    struct get_default_byte {
        static constexpr std::byte value{0};
    };

    template <typename Attr>
    struct get_default_byte<Attr, std::void_t<decltype(Attr::default_byte)>> {
        static constexpr std::byte value = Attr::default_byte;
    };

public:
    // Internal Implementation Template
    template <bool IncludeUnselected, type::NameTag... SelectedFields>
    class BuilderImpl {
        // Resolve requested fields from the main AccessorList
        using AllFieldsTuple = std::tuple<Fields...>;
        
        template <type::NameTag N>
        using ResolvedField = typename find_field<N, AllFieldsTuple>::type;

        // Verify all fields exist
        static_assert((!std::is_void_v<ResolvedField<SelectedFields>> && ...), 
            "One or more requested fields were not found in the AccessorList.");

        // Identify which fields are selected to filter unselected ones
        template <type::NameTag N>
        static constexpr bool IsSelected = ((N.toString() == SelectedFields.toString()) || ...);

        // Helper to get size of unselected fields
        template <typename F>
        static constexpr size_t GetUnselectedSize() {
            if constexpr (!IsSelected<F::name>) {
                return F::size;
            }
            return 0;
        }

        static constexpr size_t selected_size = (ResolvedField<SelectedFields>::size + ... + 0);
        
        static constexpr size_t unselected_size = []() {
            if constexpr (IncludeUnselected) {
                return (GetUnselectedSize<Fields>() + ... + 0);
            } else {
                return size_t{0};
            }
        }();
        
        static constexpr size_t raw_size = selected_size + unselected_size;

    public:
        // Round up to multiple of 8
        static constexpr size_t SIZE = (raw_size + 7) & ~7;

        explicit BuilderImpl(const T& source) : _source(source) {}

        void make(std::byte* destination) const noexcept {
            std::byte* current = destination;

            // 1. Insert Selected Fields
            ([&]() {
                using F = ResolvedField<SelectedFields>;
                typename F::inserter_type()(this->_source, current);
                current += F::size;
            }(), ...);

            // 2. Insert Unselected Fields (Padding) if requested
            if constexpr (IncludeUnselected) {
                 ([&]() {
                     using F = Fields;
                     if constexpr (!IsSelected<F::name>) {
                         constexpr std::byte pad = get_default_byte<F>::value;
                         std::memset(current, static_cast<int>(pad), F::size);
                         current += F::size;
                     }
                 }(), ...);
            }

            // 3. Zero out remaining alignment padding
            size_t bytes_written = current - destination;
            if (bytes_written < SIZE) {
                std::memset(current, 0, SIZE - bytes_written);
            }
        }

        static constexpr bool matchList(std::string_view list) noexcept {
            // Check 1: Parse input list and verify every item is in SelectedFields
            std::string_view remaining = list;
            size_t found_count = 0;

            while (!remaining.empty()) {
                size_t pos = remaining.find(',');
                std::string_view token = (pos == std::string_view::npos) 
                    ? remaining 
                    : remaining.substr(0, pos);
                
                token = trim(token);
                
                if (!token.empty()) {
                    bool found = ((token == SelectedFields.toString()) || ...);
                    if (!found) return false;
                    found_count++;
                }

                if (pos == std::string_view::npos) break;
                remaining.remove_prefix(pos + 1);
            }

            // Check 2: Verify counts match
            return found_count == sizeof...(SelectedFields);
        }

    private:
        const T& _source;
    };

    // Helper to determine the Base type for Builder
    template <BuilderArg... Args>
    struct BaseComputer {
        static constexpr auto args_tuple = std::make_tuple(Args...);
        static constexpr bool has_bool = (sizeof...(Args) > 0) && std::get<0>(args_tuple).is_bool;

        static constexpr auto GetType() {
             if constexpr (has_bool) {
                constexpr bool include_unselected = std::get<0>(args_tuple).b_val;
                return []<size_t... I>(std::index_sequence<I...>) {
                    constexpr auto tuple_ref = std::make_tuple(Args...);
                    return BuilderImpl<
                        include_unselected,
                        std::get<I + 1>(tuple_ref).asNameTag()...
                    >{ *(T*)nullptr };
                }(std::make_index_sequence<sizeof...(Args) - 1>{});
            } else {
                return []<size_t... I>(std::index_sequence<I...>) {
                    constexpr auto tuple_ref = std::make_tuple(Args...);
                    return BuilderImpl<
                        false,
                        std::get<I>(tuple_ref).asNameTag()...
                    >{ *(T*)nullptr };
                }(std::make_index_sequence<sizeof...(Args)>{});
            }
        }
        
        using type = decltype(GetType());
    };

    // Primary Template using BuilderArg
    template <BuilderArg... Args>
    class Builder : public BaseComputer<Args...>::type {
        using Base = typename BaseComputer<Args...>::type;
    public:
        using Base::Base;
    };
};

} // namespace hw::utility
