#pragma once

#include <unordered_map>
#include "Type.h"

struct FileCounter
{
	std::unordered_map<Ino, uint32_t> counter;
	void add(Ino ino);
	void remove(Ino ino);
	bool empty(Ino ino) const;
};