
#include "pch.h"
#include "BackgroundTaskManager.h"

BackgroundTaskManager::BackgroundTaskManager(int N)
	: m_executor(N)
{
}

BackgroundTaskManager::~BackgroundTaskManager()
{
}

void BackgroundTaskManager::Process()
{
	std::unique_lock lock(m_callbackMutex);

	if (m_callbackQueue.empty())
		return;

	std::vector<std::function<void()>> callbacks;
	std::swap(callbacks, m_callbackQueue);

	lock.unlock();

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
