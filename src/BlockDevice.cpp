#include "BlockDevice.h"
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include "Type.h"
#include "Errors.h"
#include "Constants.h"

BlockDevice::BlockDevice(): devicePath(""), fd(-1), isInTransaction(false) {}
BlockDevice::BlockDevice(const std::string &path): devicePath(path), fd(-1), isInTransaction(false) {}

void BlockDevice::setDevicePath(const std::string &path)
{
	devicePath = path;
}

ErrorCode BlockDevice::open()
{
	fd = ::open(devicePath.c_str(), O_RDWR);
	if (fd < 0)
	{
		return ERROR_OPEN_DEVICE_FAIL;
	}
	return SUCCESS;
}

ErrorCode BlockDevice::close()
{
	if (::close(fd) < 0)
	{
		return ERROR_CLOSE_DEVICE_FAIL;
	}
	return SUCCESS;
}

void BlockDevice::setBlockSize(uint16_t size)
{
	blockSize = size;
}

void BlockDevice::setZoneSize(uint32_t size)
{
	zoneSize = size;
}

ErrorCode BlockDevice::readBytes(uint64_t offset, void* buffer, size_t size)
{
	if (size == 0)
	{
		return SUCCESS;
	}
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
	ssize_t result = pread(fd, buffer, size, offset);
	int retries = 0;
	int nowCount = result > 0 ? static_cast<int>(result) : 0;
	while ((result < 0 || nowCount < size) && retries < MAX_READ_RETRIES)
	{
		result = pread(fd, static_cast<uint8_t*>(buffer) + nowCount, size - nowCount, offset + nowCount);
		if (result > 0)
		{
			nowCount += static_cast<int>(result);
		}
		retries++;
	}
	if (nowCount != static_cast<int>(size))
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}

ErrorCode BlockDevice::readBlock(uint32_t blockNumber, void* buffer)
{
	if (isInTransaction)
	{
		auto it = transactionWrites.find(blockNumber);
		if (it != transactionWrites.end())
		{
			memcpy(buffer, it->second.data(), blockSize);
			return SUCCESS;
		}
		ssize_t result = pread(fd, buffer, blockSize, static_cast<uint64_t>(blockNumber) * blockSize);
		if (result < blockSize)
		{
			return ERROR_READ_FAIL;
		}
		return SUCCESS;
	}
	uint64_t offset = static_cast<uint64_t>(blockNumber) * blockSize;
	return readBytes(offset, buffer, blockSize);
}

ErrorCode BlockDevice::readZone(uint32_t zoneNumber, void* buffer)
{
	if (isInTransaction)
	{
		for (Bno blockNumber = zoneNumber * (zoneSize / blockSize); blockNumber < (zoneNumber + 1) * (zoneSize / blockSize); blockNumber++)
		{
			ErrorCode err = readBlock(blockNumber, static_cast<uint8_t*>(buffer) + (blockNumber - zoneNumber * (zoneSize / blockSize)) * blockSize);
			if (err != SUCCESS)
			{
				return err;
			}
		}
		return SUCCESS;
	}
	uint64_t offset = static_cast<uint64_t>(zoneNumber) * zoneSize;
	return readBytes(offset, buffer, zoneSize);
}

ErrorCode BlockDevice::writeBytes(uint64_t offset, const void* buffer, size_t size)
{
	if (size == 0)
	{
		return SUCCESS;
	}
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
	ssize_t result = pwrite(fd, buffer, size, offset);
	int retries = 0;
	int nowCount = result > 0 ? static_cast<int>(result) : 0;
	while ((result < 0 || nowCount < size) && retries < MAX_READ_RETRIES)
	{
		result = pwrite(fd, static_cast<const uint8_t*>(buffer) + nowCount, size - nowCount, offset + nowCount);
		if (result > 0)
		{
			nowCount += static_cast<int>(result);
		}
		retries++;
	}
	if (nowCount != static_cast<int>(size))
	{
		return ERROR_WRITE_FAIL;
	}
	return SUCCESS;
}

ErrorCode BlockDevice::writeBlock(uint32_t blockNumber, const void* buffer)
{
	if (isInTransaction)
	{
		transactionWrites[blockNumber] = std::vector<uint8_t>(static_cast<const uint8_t*>(buffer), static_cast<const uint8_t*>(buffer) + blockSize);
		return SUCCESS;
	}
	uint64_t offset = static_cast<uint64_t>(blockNumber) * blockSize;
	return writeBytes(offset, buffer, blockSize);
}

ErrorCode BlockDevice::writeZone(uint32_t zoneNumber, const void* buffer)
{
	if (isInTransaction)
	{
		for (Bno blockNumber = zoneNumber * (zoneSize / blockSize); blockNumber < (zoneNumber + 1) * (zoneSize / blockSize); blockNumber++)
		{
			ErrorCode err = writeBlock(blockNumber, static_cast<const uint8_t*>(buffer) + (blockNumber - zoneNumber * (zoneSize / blockSize)) * blockSize);
			if (err != SUCCESS)
			{
				return err;
			}
		}
		return SUCCESS;
	}
	uint64_t offset = static_cast<uint64_t>(zoneNumber) * zoneSize;
	return writeBytes(offset, buffer, zoneSize);
}

ErrorCode BlockDevice::fdatasync()
{
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
	if (::fdatasync(fd) < 0)
	{
		return ERROR_WRITE_FAIL;
	}
	return SUCCESS;
}

ErrorCode BlockDevice::fsync()
{
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
	if (::fsync(fd) < 0)
	{
		return ERROR_WRITE_FAIL;
	}
	return SUCCESS;
}

ErrorCode BlockDevice::beginTransaction()
{
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
	isInTransaction = true;
	transactionWrites.clear();
	return SUCCESS;
}

ErrorCode BlockDevice::revertTransaction()
{
	if (!isInTransaction)
	{
		return ERROR_FS_BROKEN;
	}
	transactionWrites.clear();
	isInTransaction = false;
	return SUCCESS;
}

ErrorCode BlockDevice::commitTransaction()
{
	if (!isInTransaction)
	{
		return ERROR_FS_BROKEN;
	}
	isInTransaction = false;
	static uint8_t writeBuffer[ONETIME_MAX_WRITE_SIZE];
	uint32_t bufferOffset = 0;
	Bno startBlock = std::numeric_limits<Bno>::max();
	Bno lstBlock = startBlock;
	for (const auto& [blockNumber, data] : transactionWrites)
	{
		if (bufferOffset + blockSize > ONETIME_MAX_WRITE_SIZE || blockNumber != lstBlock + 1)
		{
			if (bufferOffset > 0)
			{
				ErrorCode err = writeBytes(static_cast<uint64_t>(startBlock) * blockSize, writeBuffer, bufferOffset);
				if (err != SUCCESS)
				{
					isInTransaction = true;
					return err;
				}
			}
			bufferOffset = 0;
			startBlock = blockNumber;
		}
		std::memcpy(writeBuffer + bufferOffset, data.data(), blockSize);
		bufferOffset += blockSize;
		lstBlock = blockNumber;
	}
	if (bufferOffset > 0)
	{
		ErrorCode err = writeBytes(static_cast<uint64_t>(startBlock) * blockSize, writeBuffer, bufferOffset);
		if (err != SUCCESS)
		{
			isInTransaction = true;
			return err;
		}
	}
	transactionWrites.clear();
	isInTransaction = false;
	return SUCCESS;
}