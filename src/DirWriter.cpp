#include "DirWriter.h"
#include "Constants.h"
#include "Utils.h"
#include <cstring>

void DirWriter::setFileWriter(FileWriter &fileWriter)
{
	this->fileWriter = &fileWriter;
}

void DirWriter::setDirReader(DirReader &dirReader)
{
	this->dirReader = &dirReader;
}

void DirWriter::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void DirWriter::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

ErrorCode DirWriter::addDirEntry(Ino dirInodeNumber, Ino entryInodeNumber, const std::string &entryName, uint32_t &outEntryIndex)
{
	MinixInode3 dirInode;
	ErrorCode err = inodeReader->readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!dirInode.isDirectory())
	{
		return ERROR_NOT_DIRECTORY;
	}
	if (entryName.size() == 0 || entryName.size() > MINIX3_DIR_NAME_MAX)
	{
		return ERROR_NAME_LENGTH_EXCEEDED;
	}
	if (entryInodeNumber == 0)
	{
		return ERROR_INVALID_INODE_NUMBER;
	}
	uint32_t dirSize = dirInode.i_size;
	uint32_t totalEntries = dirSize / sizeof(DirEntryOnDisk);
	std::vector<DirEntry> entries = dirReader->readDir(dirInodeNumber, 0, totalEntries, err, true);
	if (err != SUCCESS)
	{
		return err;
	}
	for(uint32_t i = 0; i < entries.size(); i++)
	{
		if (entries[i].raw.d_inode != 0)
		{
			std::string existingEntryName = char60ToString(entries[i].raw.d_name);
			if (existingEntryName == entryName)
			{
				return ERROR_FILE_NAME_EXISTS;
			}
		}
	}
	for (uint32_t i = 0; i < entries.size(); i++)
	{
		if (entries[i].raw.d_inode == 0)
		{
			outEntryIndex = i;
			return writeDirEntry(dirInodeNumber, i, entryInodeNumber, entryName);
		}
	}
	dirInode.i_size += sizeof(DirEntryOnDisk);
	err = inodeWriter->writeInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		return err;
	}
	outEntryIndex = entries.size();
	return writeDirEntry(dirInodeNumber, outEntryIndex, entryInodeNumber, entryName);
}

ErrorCode DirWriter::removeDirEntry(Ino dirInodeNumber, uint32_t entryIndex)
{
	return writeDirEntry(dirInodeNumber, entryIndex, 0, "removed");
}

ErrorCode DirWriter::writeDirEntry(Ino dirInodeNumber, uint32_t entryIndex, Ino entryInodeNumber, const std::string &entryName)
{
	MinixInode3 dirInode;
	ErrorCode err = inodeReader->readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!dirInode.isDirectory())
	{
		return ERROR_NOT_DIRECTORY;
	}
	if (entryName.size() == 0 || entryName.size() > MINIX3_DIR_NAME_MAX)
	{
		return ERROR_NAME_LENGTH_EXCEEDED;
	}
	if (entryIndex * sizeof(DirEntryOnDisk) >= dirInode.i_size)
	{
		return ERROR_INVALID_FILE_OFFSET;
	}
	DirEntryOnDisk entryOnDisk;
	entryOnDisk.d_inode = entryInodeNumber;
	std::memset(entryOnDisk.d_name, 0, MINIX3_DIR_NAME_MAX);
	std::memcpy(entryOnDisk.d_name, entryName.c_str(), entryName.size());
	return fileWriter->writeFile(dirInodeNumber, reinterpret_cast<const uint8_t*>(&entryOnDisk), entryIndex * sizeof(DirEntryOnDisk), sizeof(DirEntryOnDisk));
}