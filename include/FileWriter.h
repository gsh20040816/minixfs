#pragma once

#include "BlockDevice.h"
#include "Allocator.h"
#include "FileMapper.h"
#include "InodeWriter.h"
#include "Layout.h"
#include <cstdint>

struct FileWriter
{
	BlockDevice *blockDevice;
	FileMapper *fileMapper;
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	Layout *layout;
	void setBlockDevice(BlockDevice &blockDevice);
	void setFileMapper(FileMapper &fileMapper);
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	void setLayout(Layout &layout);
	ErrorCode writeFile(Ino inodeNumber, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite);
};