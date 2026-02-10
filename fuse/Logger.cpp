#include "Logger.h"
#include <cstdio>

#ifndef MINIXFS_LOG_MIN_LEVEL
#define MINIXFS_LOG_MIN_LEVEL 1
#endif

void Logger::log(const std::string &message, LogLevel level)
{
	if (static_cast<int>(level) < MINIXFS_LOG_MIN_LEVEL)
	{
		return;
	}
	const char *levelStr = "";
	switch (level)
	{
	case LOG_DEBUG:
		levelStr = "DEBUG";
		break;
	case LOG_INFO:
		levelStr = "INFO";
		break;
	case LOG_ERROR:
		levelStr = "ERROR";
		break;
	default:
		levelStr = "UNKNOWN";
		break;
	}
	std::fprintf(stderr, "[%s] %s\n", levelStr, message.c_str());
}
