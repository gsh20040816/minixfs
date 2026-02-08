#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "Errors.h"
#include "Type.h"

class BlockDevice
{
private:
	std::string devicePath;
	int fd;
	uint16_t blockSize;
	uint32_t zoneSize;
	bool isInTransaction;
	std::unordered_map<Bno, std::vector<uint8_t>> transactionWrites;
	const int MAX_READ_RETRIES = 3;
public:
	BlockDevice();
	BlockDevice(const std::string &path);
	void setDevicePath(const std::string &path);
	void setBlockSize(uint16_t size);
	void setZoneSize(uint32_t size);
	ErrorCode open();
	ErrorCode close();
	ErrorCode readBytes(uint64_t offset, void* buffer, size_t size);
	ErrorCode readBlock(uint32_t blockNumber, void* buffer);
	ErrorCode readZone(uint32_t zoneNumber, void* buffer);
	ErrorCode writeBytes(uint64_t offset, const void* buffer, size_t size);
	ErrorCode writeBlock(uint32_t blockNumber, const void* buffer);
	ErrorCode writeZone(uint32_t zoneNumber, const void* buffer);
	ErrorCode startTransaction();
	ErrorCode revertTransaction();
	ErrorCode commitTransaction();
};