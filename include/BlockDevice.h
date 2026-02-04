#pragma once

#include <string>
#include <cstdint>
#include "Errors.h"

class BlockDevice
{
private:
	std::string devicePath;
	int fd;
	uint16_t blockSize;
	const int MAX_READ_RETRIES = 3;
public:
	BlockDevice(const std::string &path);
	void setBlockSize(uint16_t size);
	ErrorCode open();
	ErrorCode close();
	ErrorCode readBytes(uint64_t offset, void* buffer, size_t size);
	ErrorCode readBlock(uint32_t blockNumber, void* buffer);
};