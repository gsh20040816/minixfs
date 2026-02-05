#include "InodeReader.h"
#include "Constants.h"
#include "Inode.h"
#include <cstring>

void InodeReader::setLayout(Layout &layout)
{
	this->layout = &layout;
}

void InodeReader::setBlockDevice(BlockDevice &blockDevice)
{
	this->blockDevice = &blockDevice;
}

ErrorCode InodeReader::readInode(Ino inodeNumber, void* buffer)
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
	memcpy(buffer, static_cast<uint8_t*>(blockBuffer) + inodeOffset.offsetInBlock, MINIX3_INODE_SIZE);
	free(blockBuffer);
	return SUCCESS;
}

Attribute InodeReader::readAttribute(Ino inodeNumber, ErrorCode &outError)
{
	Attribute attr = {};
	MinixInode3 inode;
	ErrorCode err = readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		outError = err;
		return attr;
	}
	attr.ino = inodeNumber;
	attr.mode = inode.i_mode;
	attr.size = inode.isRegularFile() || inode.isDirectory() ? inode.i_size : 0;
	attr.nlinks = inode.i_nlinks;
	attr.uid = inode.i_uid;
	attr.gid = inode.i_gid;
	attr.atime = inode.i_atime;
	attr.mtime = inode.i_mtime;
	attr.ctime = inode.i_ctime;
	attr.blocks = (inode.i_size + layout->blockSize - 1) / layout->blockSize;
	attr.rdev = 0;
	outError = SUCCESS;
	return attr;
}