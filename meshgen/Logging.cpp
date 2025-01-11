
#include "pch.h"
#include "Logging.h"

#include "meshgen/Application.h"
#include "meshgen/ApplicationConfig.h"
#include "meshgen/ConsolePanel.h"
#include "eqglib/eqglib.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>

#include <filesystem>

namespace fs = std::filesystem;

std::shared_ptr<spdlog::logger> g_logger;

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void Logging::Initialize()
{
	// Set up our console logging sink
	g_consoleSink = std::make_shared<ConsoleLogSink>();

	// set up default logger
	g_logger = std::make_shared<spdlog::logger>("MeshGen");
	g_logger->set_level(spdlog::level::trace);
	g_logger->sinks().push_back(g_consoleSink);

#if defined(_DEBUG)
	{
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
		sink->set_level(spdlog::level::debug);
		g_logger->sinks().push_back(sink);
	}
#endif

	spdlog::set_default_logger(g_logger);
	spdlog::set_pattern("%L %Y-%m-%d %T.%f [%n] %v");
	spdlog::flush_every(std::chrono::seconds{ 5 });

	auto recastLogger = g_logger->clone("Recast");
	recastLogger->set_level(spdlog::level::trace);
	spdlog::register_logger(recastLogger);

	auto bgfxLogger = g_logger->clone("bgfx");
	bgfxLogger->set_level(spdlog::level::debug);
	spdlog::register_logger(bgfxLogger);

	auto eqgLogger = g_logger->clone("EQG");
	eqgLogger->set_level(spdlog::level::trace);
	spdlog::register_logger(eqgLogger);
	eqg::set_logger(eqgLogger);
}

void Logging::InitSecondStage(Application* app)
{
	fs::path logsPath = g_config.GetLogsPath();
	fs::path logFile = logsPath / "MeshGenerator.log";

	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile.string(), true);
	sink->set_level(spdlog::level::debug);
	g_logger->sinks().push_back(sink);
}

void Logging::Shutdown()
{
	spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) {
		std::erase(logger->sinks(), g_consoleSink);
	});
	g_consoleSink.reset();
	g_logger.reset();
	eqg::set_logger();
}

std::shared_ptr<spdlog::logger> Logging::GetLogger(LoggingCategory category)
{
	switch (category)
	{
	case LoggingCategory::EQG:
		return spdlog::get("EQG");

	case LoggingCategory::Recast:
		return spdlog::get("Recast");

	case LoggingCategory::Bgfx:
		return spdlog::get("bgfx");

	default:
		return g_logger;
	}
}

LoggingCategory Logging::GetLoggingCategory(std::string_view loggerName)
{
	if (loggerName.empty())
		return LoggingCategory::MeshGen;

	switch (loggerName[0])
	{
	case 'E':
		if (loggerName == "EQG")
			return LoggingCategory::EQG;
		return LoggingCategory::Other;

	case 'R':
		if (loggerName == "Recast")
			return LoggingCategory::Recast;
		return LoggingCategory::Other;

	case 'M':
		if (loggerName == "MeshGen")
			return LoggingCategory::MeshGen;
		return LoggingCategory::Other;

	case 'b':
		if (loggerName == "bgfx")
			return LoggingCategory::Bgfx;
		return LoggingCategory::Other;

	default:
		return LoggingCategory::Other;
	}
}

const char* Logging::GetLoggingCategoryName(LoggingCategory category)
{
	switch (category)
	{
	case LoggingCategory::Recast: return "Recast";
	case LoggingCategory::EQG: return "EQG";
	case LoggingCategory::MeshGen: return "MeshGen";
	case LoggingCategory::Bgfx: return "Bgfx";
	default:
	case LoggingCategory::Other: return "Other";
	}
}
