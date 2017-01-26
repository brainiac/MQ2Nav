//
// enum.h
//

#pragma once

#include <iomanip>
#include <ostream>
#include <sstream>
#include <type_traits>

// -----------------------------------------------------------------------------
// Traits for treating enums like flags with bitwise operations

template <typename E, typename = std::enable_if_t<std::is_enum<E>::value>>
constexpr bool has_bitwise_operations(E) { return false; }

// -----------------------------------------------------------------------------
// bitwise OR

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline E& operator|=(E& a, E b)
{
	using UT = std::underlying_type_t<E>;
	return a = static_cast<E>(static_cast<UT>(a) | static_cast<UT>(b));
}

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline E operator|(E a, E b)
{
	return a |= b;
}

// -----------------------------------------------------------------------------
// bitwise AND

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline E& operator&=(E& a, E b)
{
	using UT = std::underlying_type_t<E>;
	return a = static_cast<E>(static_cast<UT>(a) & static_cast<UT>(b));
}

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline E operator&(E a, E b)
{
	return a &= b;
}

// -----------------------------------------------------------------------------
// bitwise XOR

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline E& operator^=(E& a, E b)
{
	using UT = std::underlying_type_t<E>;
	return a = static_cast<E>(static_cast<UT>(a) ^ static_cast<UT>(b));
}

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline E operator^(E a, E b)
{
	return a ^= b;
}

// -----------------------------------------------------------------------------
// bitwise NOT

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{}) >>
	constexpr inline E operator~(E a)
{
	using UT = std::underlying_type_t<E>;
	return static_cast<E>(~static_cast<UT>(a));
}

// -----------------------------------------------------------------------------
// convenient conversions to bool or underlying integral type (and thence 
// implicitly to bool)

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline bool operator!(E a)
{
	return a == E{};
}

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
constexpr inline std::underlying_type_t<E> operator+(E a)
{
	using UT = std::underlying_type_t<E>;
	return static_cast<UT>(a);
}

// -----------------------------------------------------------------------------
// output as hex

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
inline std::ostream& operator<<(std::ostream& os, E a)
{
	using UT = std::underlying_type_t<E>;
	auto prev_fill = os.fill('0');
	auto prev_base = os.setf(std::ios_base::hex);
	os << "0x" << std::setw(sizeof(UT)*2) << static_cast<UT>(a);
	os.setf(prev_base, std::ios_base::basefield);
	os.fill(prev_fill);
	return os;
}

template <typename E, typename = std::enable_if_t<has_bitwise_operations(E{})>>
inline std::string to_string(E a)
{
	std::stringstream ss;
	ss << a;
	return ss.str();
}
