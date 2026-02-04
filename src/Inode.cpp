#include "Inode.h"
#include <sys/stat.h>

bool MinixInode3::isFifo() const
{
	return S_ISFIFO(i_mode);
}

bool MinixInode3::isCharacterDevice() const
{
	return S_ISCHR(i_mode);
}

bool MinixInode3::isDirectory() const
{
	return S_ISDIR(i_mode);
}

bool MinixInode3::isBlockDevice() const
{
	return S_ISBLK(i_mode);
}

bool MinixInode3::isRegularFile() const
{
	return S_ISREG(i_mode);
}

bool MinixInode3::isSymbolicLink() const
{
	return S_ISLNK(i_mode);
}

bool MinixInode3::isSocket() const
{
	return S_ISSOCK(i_mode);
}