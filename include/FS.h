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
	uint16_t g_BlockSize;
	uint32_t g_ZoneSize;
	Bno g_InodesBitmapStart;
	Bno g_ZonesBitmapStart;
	Bno g_InodesTableStart;
	Bno g_DataZonesStart;
	uint32_t g_InodesPerBlock;
	uint32_t g_BlocksPerZone;
	uint32_t g_IndirectZonesPerBlock;

	FS(const std::string &devicePath);
	ErrorCode mount();
	ErrorCode unmount();
	ErrorCode readInode(Ino inodeNumber, void *buffer);
	ErrorCode readOneZoneData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead);
	ErrorCode readSingleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead);
	ErrorCode readDoubleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead);
	ErrorCode readTripleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead);
	ErrorCode readInodeFullData(Ino inodeNumber, uint8_t *buffer);
	Ino getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name);
	Ino getInodeFromPath(const std::string &path);
};