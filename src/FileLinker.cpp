#include "FileLinker.h"

void FileLinker::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void FileLinker::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

void FileLinker::setDirWriter(DirWriter &dirWriter)
{
	this->dirWriter = &dirWriter;
}

void FileLinker::setPathResolver(PathResolver &pathResolver)
{
	this->pathResolver = &pathResolver;
}

ErrorCode FileLinker::linkFile(const Ino parentInodeNumber, const std::string &name, const Ino targetInodeNumber)
{
	ErrorCode err;
	uint32_t existingEntryIndex = pathResolver->getIdxFromParentAndName(parentInodeNumber, name, err);
	if (err != SUCCESS && err != ERROR_FILE_NOT_FOUND)
	{
		return err;
	}
	if (err == SUCCESS)
	{
		return ERROR_FILE_NAME_EXISTS;
	}
	MinixInode3 targetInode;
	err = inodeReader->readInode(targetInodeNumber, &targetInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (targetInode.i_nlinks == std::numeric_limits<uint16_t>::max())
	{
		return ERROR_NLINKS_EXCEEDED;
	}
	targetInode.i_nlinks++;
	err = inodeWriter->writeInode(targetInodeNumber, &targetInode);
	if (err != SUCCESS)
	{
		return err;
	}
	err = dirWriter->addDirEntry(parentInodeNumber, targetInodeNumber, name, existingEntryIndex);
	if (err != SUCCESS)
	{
		if (targetInode.i_nlinks == 0)
		{
			return ERROR_FS_BROKEN;
		}
		targetInode.i_nlinks--;
		inodeWriter->writeInode(targetInodeNumber, &targetInode);
		return err;
	}
	return SUCCESS;
}