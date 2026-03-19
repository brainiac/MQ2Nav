#pragma once

#include <chrono>

namespace eqg {

class world_clock
{
public:
	using rep = std::uint32_t;
	using period = std::milli;
	using duration = std::chrono::duration<rep, period>;
	using time_point = std::chrono::time_point<world_clock>;
	
	static constexpr bool is_steady = true;

	static time_point now() noexcept
	{
		return current_time;
	}

	static void update(std::chrono::milliseconds elapsedMillis)
	{
		current_time += elapsedMillis;
	}

private:
	// manually advanced time point for the engine
	static inline time_point current_time{ duration{0} };
};

} // namespace eqg
