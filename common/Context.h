//
// Context.h
//

#pragma once

#ifdef ERROR
#undef ERROR
#endif

enum LogLevel
{
	VERBOSE = 0,
	DEBUG = 1,
	INFO = 2,
	WARNING = 3,
	ERROR = 4
};

class Context
{
public:
	virtual ~Context() {}

	// Log something...
	virtual void Log(LogLevel logLevel, const char* format, ...) = 0;
};
