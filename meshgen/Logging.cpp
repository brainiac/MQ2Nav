
#include "pch.h"
#include "Logging.h"

#include "Application.h"
#include "ApplicationConfig.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>
#include <zone-utilities/log/log_base.h>
#include <zone-utilities/log/log_macros.h>

static const int32_t MAX_LOG_MESSAGES = 1000;

class EQEmuLogSink : public EQEmu::Log::LogBase
{
public:
	EQEmuLogSink()
	{
		m_logger = spdlog::get("EQEmu");
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
		}
	}

private:
	std::shared_ptr<spdlog::logger> m_logger;
};

std::shared_ptr<spdlog::logger> g_logger;

void Logging::Initialize()
{
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
	spdlog::register_logger(g_logger->clone("Recast"));
	spdlog::register_logger(g_logger->clone("EQEmu"));

	eqLogInit(-1);
	eqLogRegister(std::make_shared<EQEmuLogSink>());
}

void Logging::InitSecondStage(Application* app)
{
	std::string logFile = fmt::format("{}/logs/MeshGenerator.log", g_config.GetLogsPath());

	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
	sink->set_level(spdlog::level::debug);
	g_logger->sinks().push_back(sink);
}


void Logging::Shutdown()
{
	g_logger.reset();
}


RecastContext::RecastContext()
{
	m_logger = spdlog::get("Recast");
}

void RecastContext::doLog(const rcLogCategory category, const char* message, const int length)
{
	switch (category)
	{
	case RC_LOG_PROGRESS:
		m_logger->trace(std::string_view(message, length));
		break;

	case RC_LOG_WARNING:
		m_logger->warn(std::string_view(message, length));
		break;

	case RC_LOG_ERROR:
		m_logger->error(std::string_view(message, length));
		break;
	}
}

void RecastContext::doResetTimers()
{
	for (int i = 0; i < RC_MAX_TIMERS; ++i)
		m_accTime[i] = std::chrono::nanoseconds();
}

void RecastContext::doStartTimer(const rcTimerLabel label)
{
	m_startTime[label] = std::chrono::steady_clock::now();
}

void RecastContext::doStopTimer(const rcTimerLabel label)
{
	auto deltaTime = std::chrono::steady_clock::now() - m_startTime[label];

	m_accTime[label] += deltaTime;
}

int RecastContext::doGetAccumulatedTime(const rcTimerLabel label) const
{
	return (int)std::chrono::duration_cast<std::chrono::microseconds>(
		m_accTime[label]).count();
}
