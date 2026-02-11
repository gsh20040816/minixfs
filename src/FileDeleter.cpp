#include "FileDeleter.h"

void FileDeleter::setImapAllocator(Allocator &imapAllocator)
{
	this->imapAllocator = &imapAllocator;
}

void FileDeleter::setFileWriter(FileWriter &fileWriter)
{
	this->fileWriter = &fileWriter;
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
	return imapAllocator->sync();
}