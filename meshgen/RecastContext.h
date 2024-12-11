#pragma once

#include <Recast.h>
#include <memory>

class RecastContext : public rcContext
{
public:
	RecastContext();
	virtual ~RecastContext() override;

	void ResetAllTimers();

	void LogBuildTimes(int totalTimeUsec);

protected:
	virtual void doResetLog() override {}
	virtual void doLog(rcLogCategory category, const char* message, int length) override;
	virtual void doResetTimers() override;
	virtual void doStartTimer(const rcTimerLabel label) override;
	virtual void doStopTimer(const rcTimerLabel label) override;
	virtual int doGetAccumulatedTime(const rcTimerLabel label) const override;

private:
	struct Priv;
	std::unique_ptr<Priv> m_priv;
};
