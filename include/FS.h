#pragma once

#include "BlockDevice.h"
#include "Superblock.h"
#include "Type.h"
#include "Errors.h"
#include "DirEntry.h"
#include "Attribute.h"
#include <string>
#include <cstdint>
#include <vector>
#include <sys/stat.h>

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

	ErrorCode readInode(Ino inodeNumber, void *buffer);
	ErrorCode readOneZoneData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readSingleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readDoubleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readTripleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readInodeData(Ino inodeNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readInodeFullData(Ino inodeNumber, uint8_t *buffer);
	Bno zone2Block(Zno zoneNumber);
	Ino getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name, ErrorCode &outError);
	Ino getInodeFromPath(const std::string &path, ErrorCode &outError);
	Attribute getAttributeFromInode(Ino inodeNumber, ErrorCode &outError);
public:
	FS();
	FS(const std::string &devicePath);
	void setDevicePath(const std::string &devicePath);
	ErrorCode mount();
	ErrorCode unmount();
	uint16_t getBlockSize() const;
	struct stat attrToStat(const Attribute &attr) const;
	std::vector<DirEntry> listDir(const std::string &path, uint32_t offset, uint32_t count, ErrorCode &outError);
	std::vector<DirEntry> listDir(const std::string &path, ErrorCode &outError);
	uint32_t readFile(const std::string &path, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError);
	Attribute getFileAttribute(const std::string &path, ErrorCode &outError);
};