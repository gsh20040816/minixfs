#pragma once
#include "DirWriter.h"
#include "DirReader.h"
#include "InodeReader.h"
#include <string>

struct FileCreator
{
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	DirReader *dirReader;
	DirWriter *dirWriter;
	Allocator *imapAllocator;
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	void setDirReader(DirReader &dirReader);
	void setDirWriter(DirWriter &dirWriter);
	void setImapAllocator(Allocator &imapAllocator);
	Ino createFile(Ino parentInodeNumber, const std::string &name, uint16_t mode, uint16_t uid, uint16_t gid, ErrorCode &outError);
};