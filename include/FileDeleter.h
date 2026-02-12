#pragma once

#include "Allocator.h"
#include "FileWriter.h"
#include "FileCounter.h"
#include "DirReader.h"
#include "DirWriter.h"
#include "InodeReader.h"
#include "InodeWriter.h"

struct FileDeleter
{
	Allocator *imapAllocator;
	FileWriter *fileWriter;
	FileCounter *fileCounter;
	DirReader *dirReader;
	DirWriter *dirWriter;
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	void setImapAllocator(Allocator &imapAllocator);
	void setFileWriter(FileWriter &fileWriter);
	void setFileCounter(FileCounter &fileCounter);
	void setDirReader(DirReader &dirReader);
	void setDirWriter(DirWriter &dirWriter);
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	ErrorCode deleteFile(Ino inodeNumber);
	ErrorCode unlinkFile(Ino parentInodeNumber, uint32_t idx);
};