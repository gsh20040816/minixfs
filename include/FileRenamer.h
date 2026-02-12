#pragma once
#include "InodeReader.h"
#include "DirReader.h"
#include "DirWriter.h"
#include "FileDeleter.h"
#include "PathResolver.h"

struct FileRenamer
{
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	DirReader *dirReader;
	DirWriter *dirWriter;
	FileDeleter *fileDeleter;
	PathResolver *pathResolver;
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	void setDirReader(DirReader &dirReader);
	void setDirWriter(DirWriter &dirWriter);
	void setPathResolver(PathResolver &pathResolver);
	void setFileDeleter(FileDeleter &fileDeleter);
	ErrorCode rename(Ino srcParentInodeNumber, const std::string &srcName, Ino dstParentInodeNumber, const std::string &dstName, bool failIfDstExists = false);
};