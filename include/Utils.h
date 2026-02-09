#pragma once

#include <vector>
#include <string>
#include "Errors.h"

std::vector<std::string> splitPath(const std::string &path);
std::pair<std::string, std::string> splitPathIntoDirAndBase(const std::string &path);
std::string char60ToString(const char str[60]);
int errorCodeToInt(ErrorCode code);