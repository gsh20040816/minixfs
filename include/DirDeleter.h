#pragma once

#include "InodeReader.h"
#include "InodeWriter.h"
#include "DirReader.h"
#include "FileDeleter.h"
#include "PathResolver.h"

struct DirDeleter
{
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	DirReader *dirReader;
	FileDeleter *fileDeleter;
	PathResolver *pathResolver;
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	void setDirReader(DirReader &dirReader);
	void setFileDeleter(FileDeleter &fileDeleter);
	void setPathResolver(PathResolver &pathResolver);
	ErrorCode deleteDir(Ino parentInodeNumber, const std::string &dirName);
};