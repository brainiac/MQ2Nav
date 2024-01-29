#pragma once

#include <spdlog/spdlog.h>

class Application;

extern std::shared_ptr<spdlog::logger> g_logger;

enum class LoggingCategory
{
	MeshGen = 0,
	EQEmu,
	Recast,
	Bgfx,
	Other,
};

namespace Logging
{
	// Executed at very beginning of app, before we have all path-related things figured out
	void Initialize();

	// Executed during 2nd stage of initialization
	void InitSecondStage(Application* app);

	std::shared_ptr<spdlog::logger> GetLogger(LoggingCategory category);

	LoggingCategory GetLoggingCategory(std::string_view loggerName);
	const char* GetLoggingCategoryName(LoggingCategory category);

	void Shutdown();
}

//----------------------------------------------------------------------------
