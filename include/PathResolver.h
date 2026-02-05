#pragma once

#include "DirReader.h"

struct PathResolver
{
	InodeReader *inodeReader;
	DirReader *dirReader;
	void setInodeReader(InodeReader &inodeReader);
	void setDirReader(DirReader &dirReader);
	Ino getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name, ErrorCode &outError);
	Ino resolvePath(const std::string &path, ErrorCode &outError);
};