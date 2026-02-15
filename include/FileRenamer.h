#pragma once
#include "InodeReader.h"
#include "DirReader.h"
#include "DirWriter.h"
#include "DirDeleter.h"
#include "FileDeleter.h"
#include "PathResolver.h"
#include "FileLinker.h"

struct FileRenamer
{
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	DirReader *dirReader;
	DirWriter *dirWriter;
	DirDeleter *dirDeleter;
	FileDeleter *fileDeleter;
	PathResolver *pathResolver;
	FileLinker *fileLinker;
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	void setDirReader(DirReader &dirReader);
	void setDirWriter(DirWriter &dirWriter);
	void setPathResolver(PathResolver &pathResolver);
	void setFileLinker(FileLinker &fileLinker);
	void setFileDeleter(FileDeleter &fileDeleter);
	void setDirDeleter(DirDeleter &dirDeleter);
	ErrorCode rename(Ino srcParentInodeNumber, const std::string &srcName, Ino dstParentInodeNumber, const std::string &dstName, bool failIfDstExists = false);
};