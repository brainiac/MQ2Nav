
#pragma once

#include <taskflow/taskflow.hpp>


class BackgroundTaskManager
{
public:
	explicit BackgroundTaskManager(int N = std::thread::hardware_concurrency());
	~BackgroundTaskManager();

	void Process();
	void PostToMainThread(std::function<void()> cb);

	void Stop();
	
	auto RunTask(tf::Taskflow&& tf)
	{
		return m_executor.run(std::move(tf));
	}
	
private:
	tf::Executor m_executor;
	std::mutex m_callbackMutex;
	std::vector<std::function<void()>> m_callbackQueue;
};
