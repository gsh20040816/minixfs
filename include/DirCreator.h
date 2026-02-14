#pragma once
#include "InodeReader.h"
#include "InodeWriter.h"
#include "PathResolver.h"
#include "FileLinker.h"
#include "FileCreator.h"

struct DirCreator
{
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	FileCreator *fileCreator;
	FileLinker *fileLinker;
	PathResolver *pathResolver;
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	void setPathResolver(PathResolver &pathResolver);
	void setFileCreator(FileCreator &fileCreator);
	void setFileLinker(FileLinker &fileLinker);
	ErrorCode createDir(const Ino parentInodeNumber, const std::string &name, uint16_t mode, uint16_t uid, uint16_t gid);
};