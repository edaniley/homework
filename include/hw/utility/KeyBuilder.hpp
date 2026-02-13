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
            Head::name == Name,
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

public:
    template <type::NameTag... SelectedFields>
    class Builder {
        // Resolve requested fields from the main AccessorList
        using AllFieldsTuple = std::tuple<Fields...>;

        template <type::NameTag N>
        using ResolvedField = typename find_field<N, AllFieldsTuple>::type;

        // Verify all fields exist
        static_assert((!std::is_void_v<ResolvedField<SelectedFields>> && ...),
            "One or more requested fields were not found in the AccessorList.");

        // Calculate size
        static constexpr size_t raw_size = (ResolvedField<SelectedFields>::size + ... + 0);

    public:
        // Round up to multiple of 8
        static constexpr size_t SIZE = (raw_size + 7) & ~7;

        explicit Builder(const T& source) : _source(source) {}

        void make(std::byte* destination) const noexcept {
            std::byte* current = destination;

            // Fold expression to call inserters in order
            ([&]() {
                using F = ResolvedField<SelectedFields>;
                typename F::inserter_type inserter;

                typename F::inserter_type()(this->_source, current);
                current += F::size;
            }(), ...);

            // Zero out padding
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
                    // Check if this token exists in our template parameters
                    // NameTag::toString() returns std::string_view
                    bool found = ((token == SelectedFields.toString()) || ...);
                    if (!found) return false;
                    found_count++;
                }

                if (pos == std::string_view::npos) break;
                remaining.remove_prefix(pos + 1);
            }

            // Check 2: Verify counts match (strict set equality)
            // The template parameter pack size is sizeof...(SelectedFields)
            return found_count == sizeof...(SelectedFields);
        }

    private:
        const T& _source;
    };
};

} // namespace hw::utility
