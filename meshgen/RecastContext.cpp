
#include "pch.h"
#include "RecastContext.h"
#include "meshgen/Logging.h"

#include <ranges>

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

struct RecastContext::Priv
{
	std::mutex mutex;
	std::unordered_map<std::thread::id, ThreadData> threadData;
	std::shared_ptr<spdlog::logger> logger;

	ThreadData& GetThreadData()
	{
		std::thread::id this_id = std::this_thread::get_id();

		std::unique_lock lock(mutex);
		return threadData[this_id];
	}
};

RecastContext::RecastContext()
	: m_priv(std::make_unique<Priv>())
{
	m_priv->logger = Logging::GetLogger(LoggingCategory::Recast);
}

RecastContext::~RecastContext()
{
	m_priv->logger.reset();
	m_priv->threadData.clear();
}

void RecastContext::ResetAllTimers()
{
	m_priv->threadData.clear();
}

void RecastContext::doLog(rcLogCategory category, const char* message, int length)
{
	if (!m_priv->logger)
		return;

	switch (category)
	{
	case RC_LOG_PROGRESS:
		m_priv->logger->trace(std::string_view(message, length));
		break;

	case RC_LOG_WARNING:
		m_priv->logger->warn(std::string_view(message, length));
		break;

	case RC_LOG_ERROR:
		m_priv->logger->error(std::string_view(message, length));
		break;
	}
}

void RecastContext::doResetTimers()
{
	m_priv->GetThreadData().Reset();
}

void RecastContext::doStartTimer(const rcTimerLabel label)
{
	m_priv->GetThreadData().s_startTime[label] = std::chrono::steady_clock::now();
}

void RecastContext::doStopTimer(const rcTimerLabel label)
{
	auto& threadData = m_priv->GetThreadData();

	auto deltaTime = std::chrono::steady_clock::now() - threadData.s_startTime[label];

	threadData.s_accTime[label] += deltaTime;
}

int RecastContext::doGetAccumulatedTime(const rcTimerLabel label) const
{
	std::chrono::nanoseconds total = std::chrono::nanoseconds();
	std::unique_lock lock(m_priv->mutex);

	for (auto& thread_data : m_priv->threadData | std::views::values)
	{
		total += thread_data.s_accTime[label];
	}

	return static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(total).count());
}

static void logLine(
	RecastContext& ctx, spdlog::logger& logger, rcTimerLabel label, std::string_view name, const float pc)
{
	const int t = ctx.getAccumulatedTime(label);
	if (t < 0) return;

	logger.info("{0}: {1:>{2}}ms ({3:.1f}%)", name, t / 1000.0f, 32 - name.length(), t * pc);
}

void RecastContext::LogBuildTimes(int totalTimeUsec)
{
	const float pc = 100.0f / totalTimeUsec;
	auto& logger = *m_priv->logger;

	m_priv->logger->info("Build Times");
	logLine(*this, logger, RC_TIMER_RASTERIZE_TRIANGLES,      "- Rasterize", pc);
	logLine(*this, logger, RC_TIMER_BUILD_COMPACTHEIGHTFIELD, "- Build Compact", pc);
	logLine(*this, logger, RC_TIMER_FILTER_BORDER,            "- Filter Border", pc);
	logLine(*this, logger, RC_TIMER_FILTER_WALKABLE,          "- Filter Walkable", pc);
	logLine(*this, logger, RC_TIMER_ERODE_AREA,               "- Erode Area", pc);
	logLine(*this, logger, RC_TIMER_MEDIAN_AREA,              "- Median Area", pc);
	logLine(*this, logger, RC_TIMER_MARK_BOX_AREA,            "- Mark Box Area", pc);
	logLine(*this, logger, RC_TIMER_MARK_CONVEXPOLY_AREA,     "- Mark Convex Area", pc);
	logLine(*this, logger, RC_TIMER_MARK_CYLINDER_AREA,       "- Mark Cylinder Area", pc);
	logLine(*this, logger, RC_TIMER_BUILD_DISTANCEFIELD,      "- Build Distance Field", pc);
	logLine(*this, logger, RC_TIMER_BUILD_DISTANCEFIELD_DIST, "    - Distance", pc);
	logLine(*this, logger, RC_TIMER_BUILD_DISTANCEFIELD_BLUR, "    - Blur", pc);
	logLine(*this, logger, RC_TIMER_BUILD_REGIONS,            "- Build Regions", pc);
	logLine(*this, logger, RC_TIMER_BUILD_REGIONS_WATERSHED,  "    - Watershed", pc);
	logLine(*this, logger, RC_TIMER_BUILD_REGIONS_EXPAND,     "      - Expand", pc);
	logLine(*this, logger, RC_TIMER_BUILD_REGIONS_FLOOD,      "      - Find Basins", pc);
	logLine(*this, logger, RC_TIMER_BUILD_REGIONS_FILTER,     "    - Filter", pc);
	logLine(*this, logger, RC_TIMER_BUILD_LAYERS,             "- Build Layers", pc);
	logLine(*this, logger, RC_TIMER_BUILD_CONTOURS,           "- Build Contours", pc);
	logLine(*this, logger, RC_TIMER_BUILD_CONTOURS_TRACE,     "    - Trace", pc);
	logLine(*this, logger, RC_TIMER_BUILD_CONTOURS_SIMPLIFY,  "    - Simplify", pc);
	logLine(*this, logger, RC_TIMER_BUILD_POLYMESH,           "- Build Polymesh", pc);
	logLine(*this, logger, RC_TIMER_BUILD_POLYMESHDETAIL,     "- Build Polymesh Detail", pc);
	logLine(*this, logger, RC_TIMER_MERGE_POLYMESH,           "- Merge Polymeshes", pc);
	logLine(*this, logger, RC_TIMER_MERGE_POLYMESHDETAIL,     "- Merge Polymesh Details", pc);
	m_priv->logger->info("=== TOTAL:\t{:.2f}ms", totalTimeUsec / 1000.0f);
}

