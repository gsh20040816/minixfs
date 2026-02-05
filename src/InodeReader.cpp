#include "InodeReader.h"
#include "Constants.h"
#include "Inode.h"
#include <cstring>
#include <sys/stat.h>

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

struct stat InodeReader::readStat(Ino inodeNumber, ErrorCode &outError)
{
	struct stat st;
	MinixInode3 inode;
	ErrorCode err = readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		outError = err;
		return {};
	}
	st.st_ino = inodeNumber;
	st.st_mode = inode.i_mode;
	st.st_size = inode.isRegularFile() || inode.isDirectory() ? inode.i_size : 0;
	st.st_nlink = inode.i_nlinks;
	st.st_uid = inode.i_uid;
	st.st_gid = inode.i_gid;
	st.st_atime = inode.i_atime;
	st.st_mtime = inode.i_mtime;
	st.st_ctime = inode.i_ctime;
	st.st_blocks = (inode.i_size + layout->blockSize - 1) / layout->blockSize;
	st.st_rdev = 0;
	outError = SUCCESS;
	return st;
}