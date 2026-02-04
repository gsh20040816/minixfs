#pragma once

#include "BlockDevice.h"
#include "Superblock.h"
#include "Type.h"
#include "Errors.h"
#include <string>
#include <cstdint>

class FS
{
private:
	BlockDevice g_BlockDevice;
	MinixSuperblock3 g_Superblock;
	uint32_t g_BlockSize;
	Bno g_InodesBitmapStart;
	Bno g_ZonesBitmapStart;
	Bno g_InodesTableStart;
	Bno g_DataZonesStart;
	uint32_t g_InodesPerBlock;
	uint32_t g_BlocksPerZone;
public:
	FS(const std::string &devicePath);
	ErrorCode mount();
	ErrorCode unmount();
	ErrorCode readInode(Ino inodeNumber, void* buffer);
};