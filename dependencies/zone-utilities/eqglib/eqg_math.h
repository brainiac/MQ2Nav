#pragma once

#include <concepts>

template <typename T>
concept IsArithmetic = std::floating_point<T> || std::integral<T>;

constexpr float EQ_TO_RAD(IsArithmetic auto value)
{
	// EQ uses a lookup table, we use math, but if we handle fractional parts
	// we will end up with different numbers than EQ.
	return static_cast<int>(value) * glm::pi<float>() * 0.00390625f; // 1/256
}

template <typename T, glm::qualifier Q>
constexpr glm::vec<3, T, Q> EQ_TO_RAD(const glm::vec<3, T, Q>& vec)
	requires std::floating_point<T>
{
	return glm::vec<3, T, Q>{ EQ_TO_RAD(vec.x), EQ_TO_RAD(vec.y), EQ_TO_RAD(vec.z) };
}
