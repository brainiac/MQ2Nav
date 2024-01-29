#pragma once

#include "meshgen/Logging.h"

#include <Recast.h>

#include <array>
#include <chrono>

class RecastContext : public rcContext
{
public:
	RecastContext()
	{
		m_logger = Logging::GetLogger(LoggingCategory::Recast);

		for (int i = 0; i < RC_MAX_TIMERS; ++i)
			m_accTime[i] = std::chrono::nanoseconds();
	}
	virtual ~RecastContext() override = default;

protected:
	virtual void doResetLog() override {}

	virtual void doLog(rcLogCategory category, const char* message, int length) override
	{
		switch (category)
		{
		case RC_LOG_PROGRESS:
			m_logger->trace(std::string_view(message, length));
			break;

		case RC_LOG_WARNING:
			m_logger->warn(std::string_view(message, length));
			break;

		case RC_LOG_ERROR:
			m_logger->error(std::string_view(message, length));
			break;
		}
	}

	virtual void doResetTimers() override
	{
		for (int i = 0; i < RC_MAX_TIMERS; ++i)
			m_accTime[i] = std::chrono::nanoseconds();
	}

	virtual void doStartTimer(const rcTimerLabel label) override
	{
		m_startTime[label] = std::chrono::steady_clock::now();
	}

	virtual void doStopTimer(const rcTimerLabel label) override
	{
		auto deltaTime = std::chrono::steady_clock::now() - m_startTime[label];

		m_accTime[label] += deltaTime;
	}

	virtual int doGetAccumulatedTime(const rcTimerLabel label) const override
	{
		return static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(
			m_accTime[label]).count());
	}

private:
	std::shared_ptr<spdlog::logger> m_logger;
	std::array<std::chrono::steady_clock::time_point, RC_MAX_TIMERS> m_startTime;
	std::array<std::chrono::nanoseconds, RC_MAX_TIMERS> m_accTime;
};
