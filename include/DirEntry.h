#pragma once
#include <cstdint>
#include "Type.h"
#include "Constants.h"

struct DirEntry
{
	Ino d_inode;
	char d_name[MINIX3_DIR_NAME_MAX];
}__attribute__((packed));