#pragma once

#include <type_traits>
#include <utility>
#include <string>
#include <array>

class DiskSerializable
{};

template <typename>
struct is_pair_string : std::false_type
{ };

template <typename K>
struct is_pair_string<std::pair<std::string, K>> : std::true_type
{ };

template<typename T>
inline constexpr bool is_pair_string_v = is_pair_string<T>::value;

template<typename Type>
struct is_std_array : std::false_type { };

template<typename Item, std::size_t N>
struct is_std_array< std::array<Item, N> > : std::true_type { };

template< class T >
inline constexpr bool is_std_array_v = is_std_array<T>::value;
