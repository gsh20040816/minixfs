#pragma once

#include "BlockDevice.h"
#include "Allocator.h"
#include "FileMapper.h"
#include "Layout.h"
#include <cstdint>

struct FileWriter
{
	BlockDevice *blockDevice;
	Allocator *zmapAllocator;
	FileMapper *fileMapper;
	Layout *layout;
	void setBlockDevice(BlockDevice &blockDevice);
	void setZmapAllocator(Allocator &zmapAllocator);
	void setFileMapper(FileMapper &fileMapper);
	void setLayout(Layout &layout);
	ErrorCode writeFile(const MinixInode3 &inode, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite);
};
