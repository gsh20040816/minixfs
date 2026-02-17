#pragma once

#include "Type.h"
#include "Errors.h"
#include "Layout.h"
#include "BlockDevice.h"
#include "Constants.h"

struct InodeWriter
{
	uint8_t blockBuffer[MINIX3_MAX_BLOCK_SIZE];
	Layout *layout;
	BlockDevice *blockDevice;
	void setLayout(Layout &layout);
	void setBlockDevice(BlockDevice &blockDevice);
	ErrorCode writeInode(Ino inodeNumber, void* buffer);
};