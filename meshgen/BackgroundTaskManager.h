
#pragma once

class Application;

class BackgroundTaskManager
{
public:
	BackgroundTaskManager(Application* app);
	~BackgroundTaskManager();

	void Stop();
	void StopZoneTasks();

	void BeginZoneLoad(const std::string& shortName);
	
private:
	Application* m_app;
};
