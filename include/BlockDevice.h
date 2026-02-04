#pragma once

#include <string>
#include <cstdint>
#include "Errors.h"

class BlockDevice
{
private:
	std::string devicePath;
	int fd;
public:
	BlockDevice(const std::string &path);
	ErrorCode open();
	ErrorCode close();
	ErrorCode readBlock(uint32_t blockNumber, void* buffer, size_t blockSize);
};