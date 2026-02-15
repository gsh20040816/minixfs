#include "AttributeUpdater.h"
#include "Inode.h"
#include <sys/stat.h>

void AttributeUpdater::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void AttributeUpdater::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

ErrorCode AttributeUpdater::chmod(Ino inodeNumber, uint16_t mode)
{
	MinixInode3 inode;
	ErrorCode err = inodeReader->readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	inode.i_mode = (inode.i_mode & S_IFMT) | (mode & ~S_IFMT);
	return inodeWriter->writeInode(inodeNumber, &inode);
}

ErrorCode AttributeUpdater::chown(Ino inodeNumber, uint16_t uid, uint16_t gid)
{
	MinixInode3 inode;
	ErrorCode err = inodeReader->readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	inode.i_uid = uid;
	inode.i_gid = gid;
	return inodeWriter->writeInode(inodeNumber, &inode);
}

ErrorCode AttributeUpdater::utimens(Ino inodeNumber, uint32_t atime, uint32_t mtime)
{
	MinixInode3 inode;
	ErrorCode err = inodeReader->readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	inode.i_atime = atime;
	inode.i_mtime = mtime;
	return inodeWriter->writeInode(inodeNumber, &inode);
}