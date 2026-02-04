#include "DirEntry.h"
#include <sys/stat.h>

bool DirEntry::isFifo() const
{
	return S_ISFIFO(i_mode);
}

bool DirEntry::isCharacterDevice() const
{
	return S_ISCHR(i_mode);
}

bool DirEntry::isDirectory() const
{
	return S_ISDIR(i_mode);
}

bool DirEntry::isBlockDevice() const
{
	return S_ISBLK(i_mode);
}

bool DirEntry::isRegularFile() const
{
	return S_ISREG(i_mode);
}

bool DirEntry::isSymbolicLink() const
{
	return S_ISLNK(i_mode);
}

bool DirEntry::isSocket() const
{
	return S_ISSOCK(i_mode);
}