#include "DirCreator.h"

void DirCreator::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void DirCreator::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

void DirCreator::setPathResolver(PathResolver &pathResolver)
{
	this->pathResolver = &pathResolver;
}

void DirCreator::setFileCreator(FileCreator &fileCreator)
{
	this->fileCreator = &fileCreator;
}

void DirCreator::setFileLinker(FileLinker &fileLinker)
{
	this->fileLinker = &fileLinker;
}

ErrorCode DirCreator::createDir(const Ino parentInodeNumber, const std::string &name, uint16_t mode, uint16_t uid, uint16_t gid)
{
	ErrorCode err;
	Ino newDirInodeNumber = fileCreator->createFile(parentInodeNumber, name, mode | S_IFDIR, uid, gid, err);
	if (err != SUCCESS)
	{
		return err;
	}
	err = fileLinker->linkFile(newDirInodeNumber, ".", newDirInodeNumber);
	if (err != SUCCESS)
	{
		return err;
	}
	err = fileLinker->linkFile(newDirInodeNumber, "..", parentInodeNumber);
	if (err != SUCCESS)
	{
		return err;
	}
	return SUCCESS;
}