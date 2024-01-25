#pragma once

#include <Recast.h>
#include <spdlog/spdlog.h>
#include <chrono>

class Application;


namespace Logging
{
	// Executed at very beginning of app, before we have all path-related things figured out
	void Initialize();

	// Executed during 2nd stage of initialization
	void InitSecondStage(Application* app);

	void Shutdown();
}

//----------------------------------------------------------------------------

class RecastContext : public rcContext
{
public:
	RecastContext();
	virtual ~RecastContext() = default;

protected:
	virtual void doResetLog() override {}
	virtual void doLog(const rcLogCategory category, const char* msg, const int len) override;
	virtual void doResetTimers() override;
	virtual void doStartTimer(const rcTimerLabel label) override;
	virtual void doStopTimer(const rcTimerLabel label) override;
	virtual int doGetAccumulatedTime(const rcTimerLabel label) const override;

private:
	std::shared_ptr<spdlog::logger> m_logger;
	std::array<std::chrono::steady_clock::time_point, RC_MAX_TIMERS> m_startTime;
	std::array<std::chrono::nanoseconds, RC_MAX_TIMERS> m_accTime;
};
