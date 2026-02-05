#include "DirEntry.h"
#include <sys/stat.h>

bool DirEntry::isFifo() const
{
	return S_ISFIFO(attribute.mode);
}

bool DirEntry::isCharacterDevice() const
{
	return S_ISCHR(attribute.mode);
}

bool DirEntry::isDirectory() const
{
	return S_ISDIR(attribute.mode);
}

bool DirEntry::isBlockDevice() const
{
	return S_ISBLK(attribute.mode);
}

bool DirEntry::isRegularFile() const
{
	return S_ISREG(attribute.mode);
}

bool DirEntry::isSymbolicLink() const
{
	return S_ISLNK(attribute.mode);
}

bool DirEntry::isSocket() const
{
	return S_ISSOCK(attribute.mode);
}