
#include "pch.h"
#include "Logging.h"

#include "meshgen/Application.h"
#include "meshgen/ApplicationConfig.h"
#include "meshgen/ConsolePanel.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>
#include <zone-utilities/log/log_base.h>
#include <zone-utilities/log/log_macros.h>

std::shared_ptr<spdlog::logger> g_logger;

//----------------------------------------------------------------------------

// EQEmuLogSink will capture log events from the EQEmu code (zone_utilities), and forward it to our log system.
class EQEmuLogSink : public EQEmu::Log::LogBase
{
public:
	EQEmuLogSink()
	{
		m_logger = Logging::GetLogger(LoggingCategory::EQEmu);
	}

	virtual void OnRegister(int enabled_logs) override {}
	virtual void OnUnregister() override {}

	virtual void OnMessage(EQEmu::Log::LogType log_type, const std::string& message) override
	{
		switch (log_type)
		{
		case EQEmu::Log::LogTrace:
			m_logger->trace(message);
			break;
		case EQEmu::Log::LogDebug:
			m_logger->debug(message);
			break;
		case EQEmu::Log::LogInfo:
			m_logger->info(message);
			break;
		case EQEmu::Log::LogWarn:
			m_logger->warn(message);
			break;
		case EQEmu::Log::LogError:
			m_logger->error(message);
			break;
		case EQEmu::Log::LogFatal:
			m_logger->critical(message);
			break;

		default: break;
		}
	}

private:
	std::shared_ptr<spdlog::logger> m_logger;
};

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void Logging::Initialize()
{
	// Set up our console logging sink
	g_consoleSink = std::make_shared<ConsoleLogSink>();

	// set up default logger
	g_logger = std::make_shared<spdlog::logger>("MeshGen");
	g_logger->set_level(spdlog::level::debug);

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
	spdlog::register_logger(recastLogger);

	auto emuLogger = g_logger->clone("EQEmu");
	emuLogger->set_level(spdlog::level::trace);
	spdlog::register_logger(emuLogger);

	spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) {
		logger->sinks().push_back(g_consoleSink);
	});

	eqLogInit(-1);
	eqLogRegister(std::make_shared<EQEmuLogSink>());
}

void Logging::InitSecondStage(Application* app)
{
	std::string logFile = fmt::format("{}\\MeshGenerator.log", g_config.GetLogsPath());

	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
	sink->set_level(spdlog::level::debug);
	g_logger->sinks().push_back(sink);
}

void Logging::Shutdown()
{
	spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) {
		std::erase(g_logger->sinks(), g_consoleSink);
	});
	g_consoleSink.reset();
	g_logger.reset();
}

std::shared_ptr<spdlog::logger> Logging::GetLogger(LoggingCategory category)
{
	switch (category)
	{
	case LoggingCategory::EQEmu:
		return spdlog::get("EQEmu");

	case LoggingCategory::Recast:
		return spdlog::get("Recast");

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
		if (loggerName == "EQEmu")
			return LoggingCategory::EQEmu;
		return LoggingCategory::Other;

	case 'R':
		if (loggerName == "Recast")
			return LoggingCategory::Recast;
		return LoggingCategory::Other;

	case 'M':
		if (loggerName == "MeshGen")
			return LoggingCategory::MeshGen;
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
	case LoggingCategory::EQEmu: return "EQEmu";
	case LoggingCategory::MeshGen: return "MeshGen";
	default:
	case LoggingCategory::Other: return "Other";
	}
}
