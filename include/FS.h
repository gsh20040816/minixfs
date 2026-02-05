#pragma once

#include "BlockDevice.h"
#include "Superblock.h"
#include "Type.h"
#include "Errors.h"
#include "Layout.h"
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
	Layout g_Layout;

	ErrorCode readOneZoneData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readSingleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readDoubleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readTripleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readInodeData(Ino inodeNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
	ErrorCode readInodeFullData(Ino inodeNumber, uint8_t *buffer);
	Ino getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name, ErrorCode &outError);
	Attribute getAttributeFromInode(Ino inodeNumber, ErrorCode &outError);
public:
	FS();
	FS(const std::string &devicePath);
	void setDevicePath(const std::string &devicePath);
	ErrorCode mount();
	ErrorCode unmount();
	Ino getInodeFromPath(const std::string &path, ErrorCode &outError);
	ErrorCode readInode(Ino inodeNumber, void *buffer);
	uint16_t getBlockSize() const;
	struct stat attrToStat(const Attribute &attr) const;
	std::vector<DirEntry> listDir(const std::string &path, uint32_t offset, uint32_t count, ErrorCode &outError);
	std::vector<DirEntry> listDir(const std::string &path, ErrorCode &outError);
	uint32_t readFile(const std::string &path, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError);
	Attribute getFileAttribute(const std::string &path, ErrorCode &outError);
};