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

ErrorCode BlockDevice::readBlock(uint32_t blockNumber, void* buffer, size_t blockSize)
{
	uint64_t offset = static_cast<uint64_t>(blockNumber) * blockSize;
	ssize_t result = pread(fd, buffer, blockSize, offset);
	if (result != static_cast<ssize_t>(blockSize))
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}