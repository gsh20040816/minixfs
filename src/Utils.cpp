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

std::pair<std::string, std::string> splitPathIntoDirAndBase(const std::string &path)
{
	size_t lastSlash = path.rfind('/');
	if (lastSlash == std::string::npos)
	{
		return {"/", path};
	}
	std::string dirPart = path.substr(0, lastSlash);
	std::string basePart = path.substr(lastSlash + 1);
	if (dirPart.empty())
	{
		dirPart = "/";
	}
	return {dirPart, basePart};
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
	case ERROR_CANNOT_ALLOCATE_BMAP:
		return -ENOSPC;
	case ERROR_FREEING_UNALLOCATED_BMAP:
		return -EIO;
	case ERROR_INVALID_BMAP_INDEX:
		return -EIO;
	case ERROR_INVALID_FILE_OFFSET:
		return -EIO;
	case ERROR_IS_IN_TRANSACTION:
		return -EIO;
	case ERROR_LINK_TOO_LONG:
		return -ENAMETOOLONG;
	case ERROR_PATH_TOO_DEEP:
		return -ENAMETOOLONG;
	case ERROR_LINK_EMPTY:
		return -EINVAL;
	case ERROR_NAME_LENGTH_EXCEEDED:
		return -ENAMETOOLONG;
	case ERROR_FILE_NAME_EXISTS:
		return -EEXIST;
	case ERROR_FREE_ZONE_FAILED:
		return -EIO;
	case ERROR_UNLINK_DIRECTORY:
		return -EISDIR;
	case ERROR_DIRECTORY_NOT_EMPTY:
		return -ENOTEMPTY;
	case ERROR_MOVE_TO_SUBDIR:
		return -EINVAL;
	case ERROR_DELETE_ROOT_DIR:
		return -EINVAL;
	case ERROR_IS_NOT_SYMBOLIC_LINK:
		return -EINVAL;
	case ERROR_LINK_DIRECTORY:
		return -EPERM;
	default:
		return -EIO;
	}
	return -EIO;
}