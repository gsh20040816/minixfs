#include "DirReader.h"
#include "Inode.h"
#include <cstring>

void DirReader::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void DirReader::setFileReader(FileReader &fileReader)
{
	this->fileReader = &fileReader;
}

ErrorCode DirReader::readDirRaw(MinixInode3 &dirInode, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	if (!dirInode.isDirectory())
	{
		return ERROR_NOT_DIRECTORY;
	}
	return fileReader->readFile(dirInode, buffer, sizeToRead, offset);
}

ErrorCode DirReader::readDirRaw(Ino dirInodeNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	MinixInode3 dirInode;
	ErrorCode err = inodeReader->readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		return err;
	}
	return readDirRaw(dirInode, buffer, sizeToRead, offset);
}

std::vector<DirEntry> DirReader::readDir(Ino dirInodeNumber, uint32_t offset, uint32_t count, ErrorCode &outError)
{
	std::vector<DirEntry> entries;
	MinixInode3 dirInode;
	ErrorCode err = inodeReader->readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		outError = err;
		return entries;
	}
	if (!dirInode.isDirectory())
	{
		outError = ERROR_NOT_DIRECTORY;
		return entries;
	}
	uint32_t dirSize = dirInode.i_size;
	if (dirSize % sizeof(DirEntryOnDisk) != 0)
	{
		outError = ERROR_FS_BROKEN;
		return entries;
	}
	uint32_t totalEntries = dirSize / sizeof(DirEntryOnDisk);
	if (offset >= totalEntries)
	{
		outError = SUCCESS;
		return entries;
	}
	uint32_t entriesToRead = std::min(count, totalEntries - offset);
	uint8_t *dirData = static_cast<uint8_t*>(malloc(entriesToRead * sizeof(DirEntryOnDisk)));
	if (dirData == nullptr)
	{
		outError = ERROR_CANNOT_ALLOCATE_MEMORY;
		return entries;
	}
	err = readDirRaw(dirInode, dirData, entriesToRead * sizeof(DirEntryOnDisk), offset * sizeof(DirEntryOnDisk));
	if (err != SUCCESS)
	{
		outError = err;
		free(dirData);
		return entries;
	}
	for (uint32_t offset = 0; offset < entriesToRead * sizeof(DirEntryOnDisk); offset += sizeof(DirEntryOnDisk))
	{
		DirEntryOnDisk *entryOnDisk = reinterpret_cast<DirEntryOnDisk*>(dirData + offset);
		if (entryOnDisk->d_inode != 0)
		{
			DirEntry entry;
			entry.raw = *entryOnDisk;
			memcpy(entry.raw.d_name, entryOnDisk->d_name, MINIX3_DIR_NAME_MAX);
			MinixInode3 entryInode;
			err = inodeReader->readInode(entryOnDisk->d_inode, &entryInode);
			if (err != SUCCESS)
			{
				free(dirData);
				outError = err;
				return entries;
			}
			entry.st = inodeReader->readStat(entryOnDisk->d_inode, err);
			if (err != SUCCESS)
			{
				free(dirData);
				outError = err;
				return entries;
			}
			entries.push_back(entry);
		}
	}
	free(dirData);
	outError = SUCCESS;
	return entries;
}