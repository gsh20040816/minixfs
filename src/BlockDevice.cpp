#include "BlockDevice.h"
#include <unistd.h>
#include <fcntl.h>
#include "Errors.h"

BlockDevice::BlockDevice(): devicePath(""), fd(-1) {}
BlockDevice::BlockDevice(const std::string &path): devicePath(path), fd(-1){}

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

ErrorCode BlockDevice::readBytes(uint64_t offset, void* buffer, size_t size)
{
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
	uint64_t offset = static_cast<uint64_t>(blockNumber) * blockSize;
	return readBytes(offset, buffer, blockSize);
}