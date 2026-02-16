#include "FileDeleter.h"

void FileDeleter::setImapAllocator(Allocator &imapAllocator)
{
	this->imapAllocator = &imapAllocator;
}

void FileDeleter::setFileWriter(FileWriter &fileWriter)
{
	this->fileWriter = &fileWriter;
}

void FileDeleter::setFileCounter(FileCounter &fileCounter)
{
	this->fileCounter = &fileCounter;
}

void FileDeleter::setDirReader(DirReader &dirReader)
{
	this->dirReader = &dirReader;
}

void FileDeleter::setDirWriter(DirWriter &dirWriter)
{
	this->dirWriter = &dirWriter;
}

void FileDeleter::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void FileDeleter::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

ErrorCode FileDeleter::deleteFile(Ino inodeNumber)
{
	ErrorCode err = fileWriter->truncateFile(inodeNumber, 0);
	if (err != SUCCESS)
	{
		return err;
	}
	err = imapAllocator->freeBmap(inodeNumber);
	if (err != SUCCESS)
	{
		return err;
	}
	return SUCCESS;
}

ErrorCode FileDeleter::unlinkFile(Ino parentInodeNumber, uint32_t idx)
{
	DirEntryOnDisk entryOnDisk;
	ErrorCode err = dirReader->readDirRaw(parentInodeNumber, reinterpret_cast<uint8_t*>(&entryOnDisk), sizeof(DirEntryOnDisk), idx * sizeof(DirEntryOnDisk));
	if (err != SUCCESS)
	{
		return err;
	}
	Ino inodeNumber = entryOnDisk.d_inode;
	MinixInode3 inode;
	err = inodeReader->readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	err = dirWriter->removeDirEntry(parentInodeNumber, idx);
	if (err != SUCCESS)
	{
		return err;
	}
	inode.i_nlinks--;
	err = inodeWriter->writeInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (inode.i_nlinks == 0 && fileCounter->empty(inodeNumber))
	{
		return deleteFile(inodeNumber);
	}
	return SUCCESS;
}