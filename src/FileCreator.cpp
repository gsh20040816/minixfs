#include "FileCreator.h"
#include "Utils.h"
#include <ctime>
#include <cstring>

void FileCreator::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void FileCreator::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

void FileCreator::setDirReader(DirReader &dirReader)
{
	this->dirReader = &dirReader;
}

void FileCreator::setDirWriter(DirWriter &dirWriter)
{
	this->dirWriter = &dirWriter;
}

void FileCreator::setImapAllocator(Allocator &imapAllocator)
{
	this->imapAllocator = &imapAllocator;
}

ErrorCode FileCreator::createFile(Ino parentInodeNumber, const std::string &name, uint16_t mode, uint16_t uid, uint16_t gid)
{
	if (name.empty() || name.length() > MINIX3_DIR_NAME_MAX)
	{
		return ERROR_NAME_LENGTH_EXCEEDED;
	}
	MinixInode3 parentInode;
	ErrorCode err = inodeReader->readInode(parentInodeNumber, &parentInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!parentInode.isDirectory())
	{
		return ERROR_NOT_DIRECTORY;
	}
	std::vector<DirEntry> entries = dirReader->readDir(parentInodeNumber, 0, parentInode.i_size / sizeof(DirEntryOnDisk), err);
	if (err != SUCCESS)
	{
		return err;
	}
	for (const DirEntry &entry : entries)
	{
		if (char60ToString(entry.raw.d_name) == name)
		{
			return ERROR_FILE_NAME_EXISTS;
		}
	}
	Ino newInodeNumber = imapAllocator->allocateBmap(err);
	if (err != SUCCESS)
	{
		return err;
	}
	MinixInode3 newInode = {};
	newInode.i_mode = mode;
	newInode.i_nlinks = 1;
	newInode.i_uid = uid;
	newInode.i_gid = gid;
	newInode.i_size = 0;
	newInode.i_atime = std::time(nullptr);
	newInode.i_mtime = newInode.i_atime;
	newInode.i_ctime = newInode.i_atime;
	memset(newInode.i_zone, 0, sizeof(newInode.i_zone));
	err = inodeWriter->writeInode(newInodeNumber, &newInode);
	if (err != SUCCESS)
	{
		imapAllocator->freeBmap(newInodeNumber);
		return err;
	}
	uint32_t entryIndex;
	err = dirWriter->addDirEntry(parentInodeNumber, newInodeNumber, name, entryIndex);
	if (err != SUCCESS)
	{
		imapAllocator->freeBmap(newInodeNumber);
		return err;
	}
	imapAllocator->sync();
	return SUCCESS;
}