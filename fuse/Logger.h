#pragma once

#include <string>

enum LogLevel
{
	LOG_DEBUG = 0,
	LOG_INFO = 1,
	LOG_ERROR = 2,
};

struct Logger
{
	static void log(const std::string &message, LogLevel level);
};