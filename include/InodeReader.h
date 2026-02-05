#pragma once

#include "Type.h"
#include "Errors.h"
#include "Layout.h"
#include "BlockDevice.h"
#include "Attribute.h"

struct InodeReader
{
	Layout *layout;
	BlockDevice *blockDevice;
	void setLayout(Layout &layout);
	void setBlockDevice(BlockDevice &blockDevice);
	ErrorCode readInode(Ino inodeNumber, void* buffer);
	Attribute readAttribute(Ino inodeNumber, ErrorCode &outError);
};