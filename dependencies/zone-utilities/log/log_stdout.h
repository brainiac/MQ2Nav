
#pragma once

#include "log_base.h"

#include <string>

namespace EQEmu::Log {

class LogStdOut : public LogBase
{
public:
	LogStdOut() {}
	virtual ~LogStdOut() {}

	virtual void OnRegister(int enabled_logs) {}
	virtual void OnUnregister() {}
	virtual void OnMessage(LogType log_types, const std::string& message);
};

} // namespace EQEmu::Log
