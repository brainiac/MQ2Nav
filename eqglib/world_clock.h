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
		return current_time_;
	}

	static void update(std::chrono::milliseconds elapsedMillis)
	{
		if (time_scale_ > 0.0f)
		{
			current_time_ += duration{ static_cast<uint32_t>(static_cast<float>(elapsedMillis.count()) * time_scale_) };
		}
		else
		{
			current_time_ += elapsedMillis;
		}
	}

	static void set_time_scale(float timeScale)
	{
		time_scale_ = timeScale;
	}

	static float time_scale()
	{
		return time_scale_;
	}

private:
	// manually advanced time point for the engine
	static inline time_point current_time_{ duration{0} };

	static inline float time_scale_ = 0;
};

} // namespace eqg
