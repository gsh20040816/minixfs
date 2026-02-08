#include "Utils.h"
#include <cerrno>

std::vector<std::string> splitPath(const std::string &path)
{
	std::vector<std::string> components;
	size_t start = 0;
	std::string component;
	while (start < path.length())
	{
		size_t end = path.find('/', start);
		if (end == std::string::npos)
		{
			end = path.length();
		}
		component = path.substr(start, end - start);
		if (!component.empty())
		{
			components.push_back(component);
		}
		start = end + 1;
	}
	return components;
}

std::string char60ToString(const char str[60])
{
	size_t length = 0;
	while (length < 60 && str[length] != '\0')
	{
		length++;
	}
	return std::string(str, length);
}

int errorCodeToInt(ErrorCode code)
{
	switch (code)
	{
	case SUCCESS:
		return 0;
	case ERROR_OPEN_DEVICE_FAIL:
		return -EIO;
	case ERROR_CLOSE_DEVICE_FAIL:
		return -EIO;
	case ERROR_READ_FAIL:
		return -EIO;
	case ERROR_WRITE_FAIL:
		return -EIO;
	case ERROR_INVALID_SUPERBLOCK:
		return -EIO;
	case ERROR_CANNOT_ALLOCATE_MEMORY:
		return -ENOMEM;
	case ERROR_FS_BROKEN:
		return -EIO;
	case ERROR_FILE_NOT_FOUND:
		return -ENOENT;
	case ERROR_NOT_REGULAR_FILE:
		return -EISDIR;
	case ERROR_NOT_DIRECTORY:
		return -ENOTDIR;
	case ERROR_INVALID_INODE_NUMBER:
		return -EIO;
	default:
		return -EIO;
	}
	return -EIO;
}