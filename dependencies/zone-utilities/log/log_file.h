
#pragma once

#include "log_base.h"

#include <string>
#include <cstdio>

namespace EQEmu::Log {

class LogFile : public LogBase
{
public:
	LogFile(std::string file_name);
	virtual ~LogFile();

	virtual void OnRegister(int enabled_logs);
	virtual void OnUnregister();
	virtual void OnMessage(LogType log_types, const std::string& message);

private:
	FILE* fp;
	std::string file_name;
};

} // namespace EQEmu::Log
