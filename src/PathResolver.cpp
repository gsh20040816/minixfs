#include "PathResolver.h"
#include "Utils.h"
#include "Constants.h"

void PathResolver::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void PathResolver::setDirReader(DirReader &dirReader)
{
	this->dirReader = &dirReader;
}

void PathResolver::setLinkReader(LinkReader &linkReader)
{
	this->linkReader = &linkReader;
}

Ino PathResolver::getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name, ErrorCode &outError)
{
	MinixInode3 parentInode;
	ErrorCode err = inodeReader->readInode(parentInodeNumber, &parentInode);
	if (err != SUCCESS)
	{
		outError = err;
		return 0;
	}
	if (!parentInode.isDirectory())
	{
		outError = ERROR_NOT_DIRECTORY;
		return 0;
	}
	uint32_t dirSize = parentInode.i_size;
	uint8_t *dirData = static_cast<uint8_t*>(malloc(dirSize));
	if (dirData == nullptr)
	{
		outError = ERROR_CANNOT_ALLOCATE_MEMORY;
		return 0;
	}
	err = dirReader->readDirRaw(parentInodeNumber, dirData, dirSize, 0);
	if (err != SUCCESS)
	{
		outError = err;
		free(dirData);
		return 0;
	}
	if (dirSize % sizeof(DirEntryOnDisk) != 0)
	{
		free(dirData);
		outError = ERROR_FS_BROKEN;
		return 0;
	}
	for (uint32_t offset = 0; offset < dirSize; offset += sizeof(DirEntryOnDisk))
	{
		DirEntryOnDisk *entry = reinterpret_cast<DirEntryOnDisk*>(dirData + offset);
		std::string entryName = char60ToString(entry->d_name);
		if (entryName == name && entry->d_inode != 0)
		{
			Ino inodeNumber = entry->d_inode;
			free(dirData);
			outError = SUCCESS;
			return inodeNumber;
		}
	}
	free(dirData);
	outError = ERROR_FILE_NOT_FOUND;
	return 0;
}

Ino PathResolver::resolvePath(const std::string &path, ErrorCode &outError, Ino currentInode, bool resolveLastLink)
{
	std::vector<std::string> components = splitPath(path);
	bool isResolveStart = false;
	if (!resolvePathInProgress)
	{
		resolvePathDepth = 0;
		resolvePathInProgress = true;
		isResolveStart = true;
	}
	Ino parentInode = currentInode;
	for (const std::string &component: components)
	{
		if (resolvePathDepth++ >= MAX_PATH_DEPTH)
		{
			outError = ERROR_PATH_TOO_DEEP;
			if (isResolveStart)
			{
				resolvePathInProgress = false;
			}
			return 0;
		}
		ErrorCode err;
		currentInode = getInodeFromParentAndName(currentInode, component, err);
		if (err != SUCCESS)
		{
			outError = err;
			if (isResolveStart)
			{
				resolvePathInProgress = false;
			}
			return 0;
		}
		if (currentInode == 0)
		{
			outError = ERROR_FILE_NOT_FOUND;
			if (isResolveStart)
			{
				resolvePathInProgress = false;
			}
			return 0;
		}
		if (!resolveLastLink && &component == &components.back())
		{
			break;
		}
		MinixInode3 inode;
		err = inodeReader->readInode(currentInode, &inode);
		if (err != SUCCESS)
		{
			outError = err;
			if (isResolveStart)
			{
				resolvePathInProgress = false;
			}
			return 0;
		}
		if (inode.isSymbolicLink())
		{
			std::string linkTarget;
			err = linkReader->readLink(currentInode, linkTarget);
			if (err != SUCCESS)
			{
				outError = err;
				if (isResolveStart)
				{
					resolvePathInProgress = false;
				}
				return 0;
			}
			if (linkTarget.empty())
			{
				outError = ERROR_LINK_EMPTY;
				if (isResolveStart)
				{
					resolvePathInProgress = false;
				}
				return 0;
			}
			currentInode = resolvePath(linkTarget, err, linkTarget[0] == '/' ? MINIX3_ROOT_INODE : parentInode, resolveLastLink);
			if (err != SUCCESS)
			{
				outError = err;
				if (isResolveStart)
				{
					resolvePathInProgress = false;
				}
				return 0;
			}
		}
		parentInode = currentInode;
	}
	outError = SUCCESS;
	if (isResolveStart)
	{
		resolvePathInProgress = false;
	}
	return currentInode;
}