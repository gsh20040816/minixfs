#pragma once

#include "Type.h"
#include "Errors.h"
#include "Layout.h"
#include "BlockDevice.h"

struct InodeReader
{
	Layout *layout;
	BlockDevice *blockDevice;
	void setLayout(Layout &layout);
	void setBlockDevice(BlockDevice &blockDevice);
	ErrorCode readInode(Ino inodeNumber, void* buffer);
	struct stat readStat(Ino inodeNumber, ErrorCode &outError);
};