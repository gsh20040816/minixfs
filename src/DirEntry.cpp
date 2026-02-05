#include "DirEntry.h"
#include <sys/stat.h>

bool DirEntry::isFifo() const
{
	return S_ISFIFO(st.st_mode);
}

bool DirEntry::isCharacterDevice() const
{
	return S_ISCHR(st.st_mode);
}

bool DirEntry::isDirectory() const
{
	return S_ISDIR(st.st_mode);
}

bool DirEntry::isBlockDevice() const
{
	return S_ISBLK(st.st_mode);
}

bool DirEntry::isRegularFile() const
{
	return S_ISREG(st.st_mode);
}

bool DirEntry::isSymbolicLink() const
{
	return S_ISLNK(st.st_mode);
}

bool DirEntry::isSocket() const
{
	return S_ISSOCK(st.st_mode);
}