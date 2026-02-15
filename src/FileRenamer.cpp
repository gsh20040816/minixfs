#include "FileRenamer.h"

void FileRenamer::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void FileRenamer::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

void FileRenamer::setDirReader(DirReader &dirReader)
{
	this->dirReader = &dirReader;
}

void FileRenamer::setDirWriter(DirWriter &dirWriter)
{
	this->dirWriter = &dirWriter;
}

void FileRenamer::setFileDeleter(FileDeleter &fileDeleter)
{
	this->fileDeleter = &fileDeleter;
}

void FileRenamer::setPathResolver(PathResolver &pathResolver)
{
	this->pathResolver = &pathResolver;
}

void FileRenamer::setFileLinker(FileLinker &fileLinker)
{
	this->fileLinker = &fileLinker;
}

void FileRenamer::setDirDeleter(DirDeleter &dirDeleter)
{
	this->dirDeleter = &dirDeleter;
}

ErrorCode FileRenamer::rename(Ino srcParentInodeNumber, const std::string &srcName, Ino dstParentInodeNumber, const std::string &dstName, bool failIfDstExists)
{
	MinixInode3 srcParentInode, dstParentInode;
	ErrorCode err = inodeReader->readInode(srcParentInodeNumber, &srcParentInode);
	if (err != SUCCESS)
	{
		return err;
	}
	err = inodeReader->readInode(dstParentInodeNumber, &dstParentInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!srcParentInode.isDirectory() || !dstParentInode.isDirectory())
	{
		return ERROR_NOT_DIRECTORY;
	}
	uint32_t srcEntryIndex = pathResolver->getIdxFromParentAndName(srcParentInodeNumber, srcName, err);
	if (err != SUCCESS)
	{
		return err;
	}
	uint32_t dstEntryIndex = pathResolver->getIdxFromParentAndName(dstParentInodeNumber, dstName, err);
	if (err != SUCCESS && err != ERROR_FILE_NOT_FOUND)
	{
		return err;
	}
	bool dstExists = (err == SUCCESS);
	if (err == SUCCESS && failIfDstExists)
	{
		return ERROR_FILE_NAME_EXISTS;
	}
	std::vector<DirEntry> srcEntries = dirReader->readDir(srcParentInodeNumber, srcEntryIndex, 1, err);
	if (err != SUCCESS)
	{
		return err;
	}
	if (srcEntries.empty())
	{
		return ERROR_FILE_NOT_FOUND;
	}
	DirEntry &srcEntry = srcEntries[0];
	Ino srcInodeNumber = srcEntry.raw.d_inode;
	MinixInode3 srcInode;
	err = inodeReader->readInode(srcInodeNumber, &srcInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (srcInode.isDirectory())
	{
		bool isDstSubdirOfSrc = pathResolver->twoInodesAreAncestor(srcInodeNumber, dstParentInodeNumber, err);
		if (err != SUCCESS)
		{
			return err;
		}
		if (isDstSubdirOfSrc)
		{
			return ERROR_MOVE_TO_SUBDIR;
		}
	}
	if (dstExists)
	{
		std::vector<DirEntry> dstEntries = dirReader->readDir(dstParentInodeNumber, dstEntryIndex, 1, err);
		if (err != SUCCESS)
		{
			return err;
		}
		if (dstEntries.empty())
		{
			return ERROR_FILE_NOT_FOUND;
		}
		DirEntry &dstEntry = dstEntries[0];
		if (srcInodeNumber == dstEntry.raw.d_inode)
		{
			return SUCCESS;
		}
		MinixInode3 dstInode;
		err = inodeReader->readInode(dstEntry.raw.d_inode, &dstInode);
		if (err != SUCCESS)
		{
			return err;
		}
		if (!srcInode.isDirectory() && dstInode.isDirectory())
		{
			return ERROR_NOT_REGULAR_FILE;
		}
		if (srcInode.isDirectory() && !dstInode.isDirectory())
		{
			return ERROR_NOT_DIRECTORY;
		}
		if (srcInode.isDirectory())
		{
			bool dstDirEmpty = dirReader->isDirEmpty(dstEntry.raw.d_inode, err);
			if (err != SUCCESS)
			{
				return err;
			}
			if (!dstDirEmpty)
			{
				return ERROR_DIRECTORY_NOT_EMPTY;
			}
		}
	}
	if (dstName.length() > MINIX3_DIR_NAME_MAX || dstName.empty())
	{
		return ERROR_NAME_LENGTH_EXCEEDED;
	}
	srcInode.i_nlinks++;
	err = inodeWriter->writeInode(srcInodeNumber, &srcInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (srcInode.isDirectory())
	{
		uint32_t oldParentEntryIndex = pathResolver->getIdxFromParentAndName(srcInodeNumber, "..", err);
		if (err != SUCCESS)
		{
			return err;
		}
		err = fileDeleter->unlinkFile(srcInodeNumber, oldParentEntryIndex);
		if (err != SUCCESS)
		{
			return err;
		}
		err = fileLinker->linkFile(srcInodeNumber, "..", dstParentInodeNumber);
		if (err != SUCCESS)
		{
			return err;
		}
	}
	if (dstExists)
	{
		if (srcInode.isDirectory())
		{
			err = dirDeleter->deleteDir(dstParentInodeNumber, dstName);
		}
		else
		{
			err = fileDeleter->unlinkFile(dstParentInodeNumber, dstEntryIndex);
		}
		if (err != SUCCESS)
		{
			return err;
		}
		err = dirWriter->writeDirEntry(dstParentInodeNumber, dstEntryIndex, srcInodeNumber, dstName);
		if (err != SUCCESS)
		{
			return err;
		}
		return fileDeleter->unlinkFile(srcParentInodeNumber, srcEntryIndex);
	}
	err = dirWriter->addDirEntry(dstParentInodeNumber, srcInodeNumber, dstName, dstEntryIndex);
	if (err != SUCCESS)
	{
		return err;
	}
	return fileDeleter->unlinkFile(srcParentInodeNumber, srcEntryIndex);
}