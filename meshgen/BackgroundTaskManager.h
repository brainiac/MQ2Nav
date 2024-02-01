
#pragma once

#include <taskflow/taskflow.hpp>

class Application;
class ZoneContext;

class BackgroundTaskManager
{
public:
	explicit BackgroundTaskManager(Application* app, int N = std::thread::hardware_concurrency());
	~BackgroundTaskManager();

	void Process();
	void PostToMainThread(std::function<void()> cb);

	void Stop();
	void StopZoneTasks();

	// TODO: Should this live here, or somewhere else?
	void BeginZoneLoad(const std::shared_ptr<ZoneContext>& zoneContext);
	
private:
	Application* m_app;
	tf::Executor m_executor;
	std::mutex m_callbackMutex;
	std::vector<std::function<void()>> m_callbackQueue;
};
