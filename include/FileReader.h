#pragma once

#include "BlockDevice.h"
#include "FileMapper.h"
#include "Layout.h"
#include "Inode.h"

struct FileReader
{
	uint8_t zoneBuffer[MINIX3_MAX_BLOCK_SIZE << MAX_LOG_ZONE_SIZE];
	BlockDevice *blockDevice;
	FileMapper *fileMapper;
	Layout *layout;
	void setBlockDevice(BlockDevice &blockDevice);
	void setFileMapper(FileMapper &fileMapper);
	void setLayout(Layout &layout);
	ErrorCode readFile(const MinixInode3 &inode, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset = 0);
};