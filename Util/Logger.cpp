#include "Logger.h"
#include <mutex>
#include <cstdarg>
#include <iostream>

static std::mutex s_LogMutex;

void TSLogger::Log(const char* format, ...)
{
	std::scoped_lock<std::mutex> lock(s_LogMutex);
	// Log the provided text
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}
