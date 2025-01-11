
#pragma once

#include "log_types.h"

#include <string>

namespace EQEmu::Log {

class LogBase
{
public:
	LogBase() {}
	virtual ~LogBase() {}

	virtual void OnRegister(int enabled_logs) = 0;
	virtual void OnUnregister() = 0;
	virtual void OnMessage(LogType log_type, const std::string& message) = 0;
};

} // namespace EQEmu::Log
