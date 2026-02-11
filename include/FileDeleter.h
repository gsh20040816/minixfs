#pragma once

#include "Allocator.h"
#include "FileWriter.h"

struct FileDeleter
{
	Allocator *imapAllocator;
	FileWriter *fileWriter;
	void setImapAllocator(Allocator &imapAllocator);
	void setFileWriter(FileWriter &fileWriter);
	ErrorCode deleteFile(Ino inodeNumber);
};