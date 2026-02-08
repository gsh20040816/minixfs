#include "InodeWriter.h"
#include "Constants.h"
#include "Inode.h"
#include <cstring>
#include <sys/stat.h>

void InodeWriter::setLayout(Layout &layout)
{
	this->layout = &layout;
}

void InodeWriter::setBlockDevice(BlockDevice &blockDevice)
{
	this->blockDevice = &blockDevice;
}

ErrorCode InodeWriter::writeInode(Ino inodeNumber, void* buffer)
{
	ErrorCode err;
	InodeOffset inodeOffset = layout->inodeOffset(inodeNumber, err);
	if (err != SUCCESS)
	{
		return err;
	}
	void* blockBuffer = malloc(layout->blockSize);
	if (blockBuffer == nullptr)
	{
		return ERROR_CANNOT_ALLOCATE_MEMORY;
	}
	err = blockDevice->readBlock(inodeOffset.blockNumber, blockBuffer);
	if (err != SUCCESS)
	{
		free(blockBuffer);
		return err;
	}
	memcpy(static_cast<uint8_t*>(blockBuffer) + inodeOffset.offsetInBlock, buffer, MINIX3_INODE_SIZE);
	err = blockDevice->writeBlock(inodeOffset.blockNumber, blockBuffer);
	free(blockBuffer);
	return err;
}