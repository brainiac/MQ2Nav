
#include "pch.h"
#include "RecastContext.h"

#include <ranges>

#include "meshgen/Logging.h"

// We only have one of these, and we use it from multiple threads, so we need to
// get a little creative to support timings across these threads.

static std::shared_ptr<spdlog::logger> s_logger;
std::mutex s_mutex;

struct ThreadData
{
	std::array<std::chrono::steady_clock::time_point, RC_MAX_TIMERS> s_startTime;
	std::array<std::chrono::nanoseconds, RC_MAX_TIMERS> s_accTime;

	ThreadData()
	{
		Reset();
	}

	void Reset()
	{
		for (int i = 0; i < RC_MAX_TIMERS; ++i)
			s_accTime[i] = std::chrono::nanoseconds();
	}
};

static std::unordered_map<std::thread::id, ThreadData> s_threadData;

static ThreadData& GetThreadData()
{
	std::thread::id this_id = std::this_thread::get_id();

	std::unique_lock lock(s_mutex);
	return s_threadData[this_id];
}

RecastContext::RecastContext()
{
	s_logger = Logging::GetLogger(LoggingCategory::Recast);
}

RecastContext::~RecastContext()
{
	s_logger.reset();
	s_threadData.clear();
}

void RecastContext::ResetAllTimers()
{
	s_threadData.clear();
}

void RecastContext::doLog(rcLogCategory category, const char* message, int length)
{
	switch (category)
	{
	case RC_LOG_PROGRESS:
		s_logger->trace(std::string_view(message, length));
		break;

	case RC_LOG_WARNING:
		s_logger->warn(std::string_view(message, length));
		break;

	case RC_LOG_ERROR:
		s_logger->error(std::string_view(message, length));
		break;
	}
}

void RecastContext::doResetTimers()
{
	GetThreadData().Reset();
}

void RecastContext::doStartTimer(const rcTimerLabel label)
{
	GetThreadData().s_startTime[label] = std::chrono::steady_clock::now();
}

void RecastContext::doStopTimer(const rcTimerLabel label)
{
	auto deltaTime = std::chrono::steady_clock::now() - GetThreadData().s_startTime[label];

	GetThreadData().s_accTime[label] += deltaTime;
}

int RecastContext::doGetAccumulatedTime(const rcTimerLabel label) const
{
	std::chrono::nanoseconds total = std::chrono::nanoseconds();

	for (auto& thread_data : s_threadData | std::views::values)
	{
		total += thread_data.s_accTime[label];
	}

	return static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(total).count());
}
