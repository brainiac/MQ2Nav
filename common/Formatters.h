
#pragma once

#include <fmt/core.h>
#include <glm/detail/type_vec1.hpp>
#include <glm/gtx/string_cast.hpp>

// glm types

template <int C, typename T, glm::qualifier Q>
struct fmt::formatter<glm::vec<C, T, Q>> : public formatter<std::string>
{
	template <typename FormatContext>
	auto format(const glm::vec<C, T, Q>& p, FormatContext& ctx) -> decltype(ctx.out())
	{
		return formatter<std::string>::format(glm::to_string(p), ctx);
	}
};

template <int X, int Y, typename T, glm::qualifier Q>
struct fmt::formatter<glm::mat<X, Y, T, Q>> : public formatter<std::string>
{
	template <typename FormatContext>
	auto format(const glm::mat<X, Y, T, Q>& p, FormatContext& ctx) -> decltype(ctx.out())
	{
		return formatter<std::string>::format(glm::to_string(p), ctx);
	}
};

template <typename T, glm::qualifier Q>
struct fmt::formatter<glm::qua<T, Q>> : public formatter<std::string>
{
	template <typename FormatContext>
	auto format(const glm::qua<T, Q>& p, FormatContext& ctx) -> decltype(ctx.out())
	{
		return formatter<std::string>::format(glm::to_string(p), ctx);
	}
};