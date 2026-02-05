#pragma once

#include "BlockDevice.h"
#include "Layout.h"
#include "Inode.h"

struct FileReader
{
	BlockDevice *blockDevice;
	Layout *layout;
	ErrorCode readOneZoneData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset);
	ErrorCode readSingleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset);
	ErrorCode readDoubleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset);
	ErrorCode readTripleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset);
	void setBlockDevice(BlockDevice &blockDevice);
	void setLayout(Layout &layout);
	ErrorCode readFile(const MinixInode3 &inode, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
};