//
// Logging.h
//

#pragma once

#include <fmt/format.h>

#include <glm/vec3.hpp>

// custom format types

namespace fmt {
	template<>
	struct formatter<glm::vec3> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const glm::vec3& p, FormatContext& ctx) {
			return format_to(ctx.begin(), "({:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z);
		}
	};

	template<>
	struct formatter<glm::vec2> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const glm::vec2& p, FormatContext& ctx) {
			return format_to(ctx.begin(), "({:.2f}, {:.2f})", p.x, p.y);
		}
	};
}
