
#pragma once

namespace EQEmu::Log {

enum LogType
{
	LogTrace = 1,
	LogDebug = 2,
	LogInfo = 4,
	LogWarn = 8,
	LogError = 16,
	LogFatal = 32,
	LogAll = 63
};

} // namespace EQEmu::Log
