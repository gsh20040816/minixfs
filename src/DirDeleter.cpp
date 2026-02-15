#include "DirDeleter.h"

void DirDeleter::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void DirDeleter::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

void DirDeleter::setDirReader(DirReader &dirReader)
{
	this->dirReader = &dirReader;
}

void DirDeleter::setFileDeleter(FileDeleter &fileDeleter)
{
	this->fileDeleter = &fileDeleter;
}

void DirDeleter::setPathResolver(PathResolver &pathResolver)
{
	this->pathResolver = &pathResolver;
}

ErrorCode DirDeleter::deleteDir(Ino parentInodeNumber, const std::string &dirName)
{
	ErrorCode err;
	Ino dirInodeNumber = pathResolver->getInodeFromParentAndName(parentInodeNumber, dirName, err);
	if (err != SUCCESS)
	{
		return err;
	}
	bool isEmpty = dirReader->isDirEmpty(dirInodeNumber, err);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!isEmpty)
	{
		return ERROR_DIRECTORY_NOT_EMPTY;
	}
	if (dirInodeNumber == MINIX3_ROOT_INODE)
	{
		return ERROR_DELETE_ROOT_DIR;
	}
	MinixInode3 dirInode;
	err = inodeReader->readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!dirInode.isDirectory())
	{
		return ERROR_NOT_DIRECTORY;
	}
	uint32_t selfEntryIndex = pathResolver->getIdxFromParentAndName(dirInodeNumber, ".", err);
	if (err != SUCCESS)
	{
		return err;
	}
	uint32_t parentEntryIndex = pathResolver->getIdxFromParentAndName(dirInodeNumber, "..", err);
	if (err != SUCCESS)
	{
		return err;
	}
	uint32_t indexInParent = pathResolver->getIdxFromParentAndName(parentInodeNumber, dirName, err);
	if (err != SUCCESS)
	{
		return err;
	}
	err = fileDeleter->unlinkFile(dirInodeNumber, selfEntryIndex);
	if (err != SUCCESS)
	{
		return err;
	}
	err = fileDeleter->unlinkFile(dirInodeNumber, parentEntryIndex);
	if (err != SUCCESS)
	{
		return err;
	}
	err = fileDeleter->unlinkFile(parentInodeNumber, indexInParent);
	if (err != SUCCESS)
	{
		return err;
	}
	return SUCCESS;
}