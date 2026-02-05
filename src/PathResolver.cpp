#include "PathResolver.h"
#include "Utils.h"

void PathResolver::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void PathResolver::setDirReader(DirReader &dirReader)
{
	this->dirReader = &dirReader;
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

Ino PathResolver::resolvePath(const std::string &path, ErrorCode &outError)
{
	Ino currentInode = MINIX3_ROOT_INODE;
	std::vector<std::string> components = splitPath(path);
	for (const std::string &component: components)
	{
		ErrorCode err;
		currentInode = getInodeFromParentAndName(currentInode, component, err);
		if (err != SUCCESS)
		{
			outError = err;
			return 0;
		}
	}
	outError = SUCCESS;
	return currentInode;
}