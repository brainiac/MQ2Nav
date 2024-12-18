
#pragma once

#include <taskflow/taskflow.hpp>

class Application;
class ZoneContext;

class BackgroundTaskManager
{
public:
	explicit BackgroundTaskManager(int N = std::thread::hardware_concurrency());
	~BackgroundTaskManager();

	void Process();
	void PostToMainThread(std::function<void()> cb);

	void Stop();
	void StopZoneTasks();
	
	void AddZoneTask(tf::Taskflow&& tf);
	
private:
	tf::Executor m_executor;
	std::mutex m_callbackMutex;
	std::vector<std::function<void()>> m_callbackQueue;
};
