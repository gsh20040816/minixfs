#pragma once

#include <cstdint>
#include "Constants.h"

struct MinixInode3
{
	uint16_t i_mode;
	uint16_t i_nlinks;
	uint16_t i_uid;
	uint16_t i_gid;
	uint32_t i_size;
	uint32_t i_atime;
	uint32_t i_mtime;
	uint32_t i_ctime;
	uint32_t i_zone[MINIX3_ZONES_PER_INODE];
	bool isFifo() const;
	bool isCharacterDevice() const;
	bool isDirectory() const;
	bool isBlockDevice() const;
	bool isRegularFile() const;
	bool isSymbolicLink() const;
	bool isSocket() const;
}__attribute__((packed));