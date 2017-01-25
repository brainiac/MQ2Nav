
#include "Application.h"

#include <Recast.h>
#include <RecastDebugDraw.h>

#include <imgui/imgui.h>
#include <zone-utilities/log/log_macros.h>
#include <zone-utilities/log/log_stdout.h>
#include <zone-utilities/log/log_file.h>

#include <memory>
#include <sstream>

// logger to log to the build context
class LogContext : public EQEmu::Log::LogBase
{
public:
	LogContext(BuildContext& ctx) : m_ctx(ctx) {}
	virtual ~LogContext() {}

	virtual void OnRegister(int enabled_logs) {}
	virtual void OnUnregister() {}
	virtual void OnMessage(EQEmu::Log::LogType log_type, const std::string &message)
	{
		char time_buffer[512];
		time_t current_time;
		struct tm *time_info;

		time(&current_time);
		time_info = localtime(&current_time);

		strftime(time_buffer, 512, "[%m/%d/%y %H:%M:%S] ", time_info);

		switch (log_type)
		{
		case EQEmu::Log::LogTrace:
			m_ctx.log(RC_LOG_PROGRESS, "[Trace]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogDebug:
			m_ctx.log(RC_LOG_PROGRESS, "[Debug]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogInfo:
			m_ctx.log(RC_LOG_PROGRESS, "[Info]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogWarn:
			m_ctx.log(RC_LOG_WARNING, "[Warn]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogError:
			m_ctx.log(RC_LOG_ERROR, "[Error]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogFatal:
			m_ctx.log(RC_LOG_ERROR, "[Fatal]%s%s\n", time_buffer, message.c_str());
			break;
		default:
			m_ctx.log(RC_LOG_PROGRESS, "[All]%s%s\n", time_buffer, message.c_str());
			break;
		}
	}

private:
	BuildContext& m_ctx;
};

class DebugLog : public EQEmu::Log::LogBase
{
public:
	DebugLog() {}

	virtual void OnRegister(int enabled_logs) {}
	virtual void OnUnregister() {}
	virtual void OnMessage(EQEmu::Log::LogType log_type, const std::string &message)
	{
		char time_buffer[512];
		time_t current_time;
		struct tm *time_info;

		time(&current_time);
		time_info = localtime(&current_time);

		strftime(time_buffer, 512, "%m/%d/%y %H:%M:%S", time_info);
		const char* logLevel = "Unknown";

		std::stringstream ss;

		switch (log_type)
		{
		case EQEmu::Log::LogTrace:
			logLevel = "Trace";
			break;
		case EQEmu::Log::LogDebug:
			logLevel = "Debug";
			break;
		case EQEmu::Log::LogInfo:
			logLevel = "Info";
			break;
		case EQEmu::Log::LogWarn:
			logLevel = "Warn";
			break;
		case EQEmu::Log::LogError:
			logLevel = "Error";
			break;
		case EQEmu::Log::LogFatal:
			logLevel = "Fatal";
			break;
		}

		ss << time_buffer << " [" << logLevel << "] " << message << "\n";
		OutputDebugStringA(ss.str().c_str());
	}
};

#define DebugHeader "[MeshGen]"

VOID DebugSpewAlways(PCHAR szFormat, ...)
{
	va_list vaList;
	va_start(vaList, szFormat);
	int len = _vscprintf(szFormat, vaList) + 1;// _vscprintf doesn't count // terminating '\0'  
	int headerlen = strlen(DebugHeader) + 1;
	size_t thelen = len + headerlen + 32;
	char *szOutput = (char *)LocalAlloc(LPTR, thelen);
	strcpy_s(szOutput, thelen, DebugHeader " ");
	vsprintf_s(szOutput + headerlen, thelen - headerlen, szFormat, vaList);
	strcat_s(szOutput, thelen, "\n");
	OutputDebugString(szOutput);
	LocalFree(szOutput);
}

int main(int argc, char* argv[])
{
	// Construct the path to the ini file
	CHAR logfilePath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, logfilePath, MAX_PATH);
	PathRemoveFileSpecA(logfilePath);
	PathAppendA(logfilePath, "MeshGenerator.log");

	eqLogInit(-1);
	eqLogRegister(std::make_shared<EQEmu::Log::LogFile>(logfilePath));
	eqLogRegister(std::make_shared<EQEmu::Log::LogStdOut>());

#if defined(DEBUG)
	eqLogRegister(std::make_shared<DebugLog>());
#endif

	std::string startingZone;
	if (argc > 1)
		startingZone = argv[1];

	Application window(startingZone);
	return window.RunMainLoop();
}
