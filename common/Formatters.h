
#pragma once

#include <fmt/core.h>
#include <glm/detail/type_vec1.hpp>
#include <glm/gtx/string_cast.hpp>

// glm types

template <glm::length_t L, typename T, glm::qualifier Q>
struct fmt::formatter<glm::vec<L, T, Q>> : fmt::formatter<T>
{
	constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
	{
		// Delegate to the float formatter for parsing
		return fmt::formatter<T>::parse(ctx);
	}

	auto format(const glm::vec<L, T, Q>& v, format_context& ctx) const
		-> format_context::iterator
	{
		auto out = ctx.out();
		*out++ = '(';

		for (glm::length_t i = 0; i < L; ++i)
		{
			if (i > 0)
			{
				*out++ = ',';
				*out++ = ' ';
			}

			// Use the inherited formatter with user-specified options
			out = fmt::formatter<T>::format(static_cast<T>(v[i]), ctx);
		}

		*out++ = ')';
		return out;
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