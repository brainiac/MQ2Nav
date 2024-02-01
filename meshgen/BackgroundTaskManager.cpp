
#include "pch.h"
#include "BackgroundTaskManager.h"

#include "meshgen/Application.h"
#include "meshgen/ZoneContext.h"

BackgroundTaskManager::BackgroundTaskManager(Application* app, int N)
	: m_app(app)
	, m_executor(N)
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

void BackgroundTaskManager::BeginZoneLoad(const std::shared_ptr<ZoneContext>& zoneContext)
{
	StopZoneTasks();

	// Start the task
	tf::Taskflow taskflow;

	taskflow.emplace([zoneContext, this]()
		{
			// Start the progress
			PostToMainThread([app = this->m_app, zoneContext]()
				{
					app->SetProgressDisplay(true);
					app->SetProgressText(fmt::format("Loading {}...", zoneContext->GetShortName()));
					app->SetProgressValue(0.0f);
				});

			if (!zoneContext->LoadZone())
			{
				PostToMainThread([app = this->m_app, zoneContext]()
				{
					app->SetProgressDisplay(false);
					app->ShowNotificationDialog("Failed To Open Zone",
						fmt::format("Failed to load zone: '{}'", zoneContext->GetShortName()));
				});

				return;
			}

			PostToMainThread([app = this->m_app]()
				{
					app->SetProgressText("Building triangle mesh...");
					app->SetProgressValue(50.0f);
				});


			if (!zoneContext->BuildTriangleMesh())
			{
				PostToMainThread([app = this->m_app]()
					{
						app->SetProgressDisplay(false);
						app->ShowNotificationDialog("Failed To Open Zone",
							"Failed to build triangle mesh");
					});
				return;
			}

			PostToMainThread([app = this->m_app, zoneContext]()
				{
					app->SetProgressDisplay(false);
					app->SetZoneContext(zoneContext);
				});
		});

	m_executor.run(std::move(taskflow));
}
