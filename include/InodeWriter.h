#pragma once

#include "Type.h"
#include "Errors.h"
#include "Layout.h"
#include "BlockDevice.h"

struct InodeWriter
{
	Layout *layout;
	BlockDevice *blockDevice;
	void setLayout(Layout &layout);
	void setBlockDevice(BlockDevice &blockDevice);
	ErrorCode writeInode(Ino inodeNumber, void* buffer);
};