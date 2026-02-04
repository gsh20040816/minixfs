#include "BlockDevice.h"
#include <unistd.h>
#include <fcntl.h>
#include "Errors.h"

BlockDevice::BlockDevice(const std::string &path): devicePath(path), fd(-1){}

ErrorCode BlockDevice::open()
{
	fd = ::open(devicePath.c_str(), O_RDWR | O_DIRECT);
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
	if (result != static_cast<ssize_t>(size))
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