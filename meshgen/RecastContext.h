#pragma once

#include <spdlog/spdlog.h>
#include <Recast.h>

class RecastContext : public rcContext
{
public:
	RecastContext();
	virtual ~RecastContext() override;

	static void ResetAllTimers();

protected:
	virtual void doResetLog() override {}
	virtual void doLog(rcLogCategory category, const char* message, int length) override;
	virtual void doResetTimers() override;
	virtual void doStartTimer(const rcTimerLabel label) override;
	virtual void doStopTimer(const rcTimerLabel label) override;
	virtual int doGetAccumulatedTime(const rcTimerLabel label) const override;

private:
	std::shared_ptr<spdlog::logger> m_logger;
};
