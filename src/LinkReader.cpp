#include "LinkReader.h"

void LinkReader::setFileReader(FileReader &fileReader)
{
	this->fileReader = &fileReader;
}

void LinkReader::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

ErrorCode LinkReader::readLink(Ino inodeNumber, std::string &outLinkTarget)
{
	MinixInode3 inode;
	ErrorCode err = inodeReader->readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!inode.isSymbolicLink())
	{
		return ERROR_FS_BROKEN;
	}
	uint32_t linkSize = inode.i_size;
	if (linkSize > MAX_LINK_SIZE)
	{
		return ERROR_LINK_TOO_LONG;
	}
	uint8_t *linkData = static_cast<uint8_t*>(calloc(linkSize + 1, sizeof(uint8_t)));
	if (linkData == nullptr)
	{
		return ERROR_CANNOT_ALLOCATE_MEMORY;
	}
	err = fileReader->readFile(inode, linkData, linkSize, 0);
	if (err != SUCCESS)
	{
		free(linkData);
		return err;
	}
	outLinkTarget = std::string(reinterpret_cast<char*>(linkData));
	free(linkData);
	return SUCCESS;
}