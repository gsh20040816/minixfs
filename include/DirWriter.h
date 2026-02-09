#pragma once

#include "FileWriter.h"
#include "DirReader.h"
#include "InodeReader.h"
#include "InodeWriter.h"

struct DirWriter
{
	FileWriter *fileWriter;
	DirReader *dirReader;
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	void setFileWriter(FileWriter &fileWriter);
	void setDirReader(DirReader &dirReader);
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	ErrorCode addDirEntry(Ino dirInodeNumber, Ino entryInodeNumber, const std::string &entryName, uint32_t &outEntryIndex);
	ErrorCode removeDirEntry(Ino dirInodeNumber, uint32_t entryIndex);
	ErrorCode writeDirEntry(Ino dirInodeNumber, uint32_t entryIndex, Ino entryInodeNumber, const std::string &entryName);
};