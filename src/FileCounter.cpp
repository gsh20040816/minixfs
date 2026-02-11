#include "FileCounter.h"

void FileCounter::add(Ino ino)
{
	++counter[ino];
}

void FileCounter::remove(Ino ino)
{
	auto it = counter.find(ino);
	if (it != counter.end())
	{
		if (--(it->second) == 0)
			counter.erase(it);
	}
}

bool FileCounter::empty(Ino ino) const
{
	auto it = counter.find(ino);
	return it == counter.end() || it->second == 0;
}