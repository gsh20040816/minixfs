#pragma once
#include <cstdint>
#include "Type.h"
#include "Constants.h"

struct DirEntryOnDisk
{
	Ino d_inode;
	char d_name[MINIX3_DIR_NAME_MAX];
}__attribute__((packed));

struct DirEntry
{
	DirEntryOnDisk *raw;
	uint16_t i_mode;
	bool isFifo() const;
	bool isCharacterDevice() const;
	bool isDirectory() const;
	bool isBlockDevice() const;
	bool isRegularFile() const;
	bool isSymbolicLink() const;
	bool isSocket() const;
};