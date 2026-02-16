#pragma once

#include "FileCreator.h"
#include "FileWriter.h"

struct SymlinkCreator
{
	FileCreator *fileCreator;
	FileWriter *fileWriter;
	void setFileCreator(FileCreator &fileCreator);
	void setFileWriter(FileWriter &fileWriter);
	Ino createSymlink(Ino parentInodeNumber, const std::string &name, const std::string &targetPath, uint16_t mod, uint16_t uid, uint16_t gid, ErrorCode &outError);
};