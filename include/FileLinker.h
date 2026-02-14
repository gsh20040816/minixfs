#pragma once

#include "InodeReader.h"
#include "InodeWriter.h"
#include "DirWriter.h"
#include "PathResolver.h"

struct FileLinker
{
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	DirWriter *dirWriter;
	PathResolver *pathResolver;
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	void setDirWriter(DirWriter &dirWriter);
	void setPathResolver(PathResolver &pathResolver);
	ErrorCode linkFile(const Ino parentInodeNumber, const std::string &name, const Ino targetInodeNumber);
};