#pragma once
#include "BlockDevice.h"
#include "Layout.h"
#include "InodeReader.h"
#include "FileReader.h"
#include "DirEntry.h"
#include <vector>

struct DirReader
{
	InodeReader *inodeReader;
	FileReader *fileReader;
	void setInodeReader(InodeReader &inodeReader);
	void setFileReader(FileReader &fileReader);
	ErrorCode readDirRaw(Ino dirInodeNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset);
	ErrorCode readDirRaw(MinixInode3 &dirInode, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset);
	std::vector<DirEntry> readDir(Ino dirInodeNumber, uint32_t offset, uint32_t count, ErrorCode &outError, bool keepInode0Entries = false);
};