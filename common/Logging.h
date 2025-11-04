//
// Logging.h
//

#pragma once

#include <fmt/format.h>

#include <glm/vec3.hpp>

// custom format types


template<>
struct fmt::formatter<glm::vec3>
{
	constexpr auto parse(format_parse_context& ctx)
		-> format_parse_context::iterator
	{
		return ctx.begin();
	}

	auto format(const glm::vec3& p, format_context& ctx) const
		-> format_context::iterator
	{
		return format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z);
	}
};

template<>
struct fmt::formatter<glm::vec2>
{
	constexpr auto parse(format_parse_context& ctx)
		-> format_parse_context::iterator
	{
		return ctx.begin();
	}

	auto format(const glm::vec2& p, format_context& ctx) const
		-> format_context::iterator
	{
		return format_to(ctx.out(), "({:.2f}, {:.2f})", p.x, p.y);
	}
};
