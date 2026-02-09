#pragma once

#include "DirReader.h"
#include "LinkReader.h"

struct PathResolver
{
	InodeReader *inodeReader;
	DirReader *dirReader;
	LinkReader *linkReader;
	uint32_t resolvePathDepth = 0;
	bool resolvePathInProgress = false;
	void setInodeReader(InodeReader &inodeReader);
	void setDirReader(DirReader &dirReader);
	void setLinkReader(LinkReader &linkReader);
	Ino getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name, ErrorCode &outError);
	Ino resolvePath(const std::string &path, ErrorCode &outError, Ino currentInode = MINIX3_ROOT_INODE);
};