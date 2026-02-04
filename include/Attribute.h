#pragma once

#include <cstdint>

struct Attribute
{
	uint32_t ino;
	uint16_t mode;
	uint32_t size;
	uint16_t nlinks;
	uint16_t uid;
	uint16_t gid;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
	uint32_t blocks;
	uint32_t rdev;
};