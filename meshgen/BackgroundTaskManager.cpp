
#include "pch.h"
#include "BackgroundTaskManager.h"

#include "PanelManager.h"
#include "meshgen/Application.h"
#include "meshgen/ZoneContext.h"

BackgroundTaskManager::BackgroundTaskManager(int N)
	: m_executor(N)
{
}

BackgroundTaskManager::~BackgroundTaskManager()
{
}

void BackgroundTaskManager::Process()
{
	if (m_callbackQueue.empty())
		return;

	std::vector<std::function<void()>> callbacks;

	{
		std::unique_lock lock(m_callbackMutex);
		std::swap(callbacks, m_callbackQueue);
	}

	for (const auto& cb : callbacks)
	{
		cb();
	}
}

void BackgroundTaskManager::PostToMainThread(std::function<void()> cb)
{
	std::unique_lock lock(m_callbackMutex);
	m_callbackQueue.push_back(std::move(cb));
}

void BackgroundTaskManager::Stop()
{
}

void BackgroundTaskManager::StopZoneTasks()
{
}

void BackgroundTaskManager::AddZoneTask(tf::Taskflow&& tf)
{
	m_executor.run(std::move(tf));
}
