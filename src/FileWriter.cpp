#include "FileWriter.h"
#include <cstring>

void FileWriter::setBlockDevice(BlockDevice &blockDevice)
{
	this->blockDevice = &blockDevice;
}

void FileWriter::setFileMapper(FileMapper &fileMapper)
{
	this->fileMapper = &fileMapper;
}

void FileWriter::setLayout(Layout &layout)
{
	this->layout = &layout;
}

void FileWriter::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void FileWriter::setInodeWriter(InodeWriter &inodeWriter)
{
	this->inodeWriter = &inodeWriter;
}

ErrorCode FileWriter::writeFile(Ino inodeNumber, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite)
{
	if (sizeToWrite == 0)
	{
		return SUCCESS;
	}
	MinixInode3 inodeForMap = {};
	ErrorCode err = inodeReader->readInode(inodeNumber, &inodeForMap);
	if (err != SUCCESS)
	{
		return err;
	}

	Zno startZoneIndex = offset / layout->zoneSize;
	Zno endZoneIndex = (offset + sizeToWrite - 1) / layout->zoneSize;
	for (Zno zoneIndex = startZoneIndex; zoneIndex <= endZoneIndex; zoneIndex++)
	{
		Zno physicalZoneIndex;
		ErrorCode err = fileMapper->mapLogicalToPhysical(inodeForMap, zoneIndex, physicalZoneIndex, true);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t writeSize = zoneIndex == startZoneIndex ? std::min(sizeToWrite, layout->zoneSize - (offset % layout->zoneSize)) : zoneIndex == endZoneIndex ? (offset + sizeToWrite - 1) % layout->zoneSize + 1 : layout->zoneSize;
		if (writeSize == layout->zoneSize)
		{
			err = blockDevice->writeZone(physicalZoneIndex, data + (zoneIndex - startZoneIndex) * layout->zoneSize - (offset % layout->zoneSize));
			if (err != SUCCESS)
			{
				return err;
			}
		}
		else
		{
			uint8_t *zoneBuffer = static_cast<uint8_t *>(malloc(layout->zoneSize));
			if (zoneBuffer == nullptr)
			{
				return ERROR_CANNOT_ALLOCATE_MEMORY;
			}
			err = blockDevice->readZone(physicalZoneIndex, zoneBuffer);
			if (err != SUCCESS)
			{
				free(zoneBuffer);
				return err;
			}
			memcpy(zoneBuffer + (zoneIndex == startZoneIndex ? (offset % layout->zoneSize) : 0), zoneIndex == startZoneIndex ? data : data + (zoneIndex - startZoneIndex) * layout->zoneSize - (offset % layout->zoneSize), writeSize);
			err = blockDevice->writeZone(physicalZoneIndex, zoneBuffer);
			free(zoneBuffer);
			if (err != SUCCESS)
			{
				return err;
			}
		}
	}
	inodeForMap.i_size = std::max(inodeForMap.i_size, offset + sizeToWrite);
	err = inodeWriter->writeInode(inodeNumber, &inodeForMap);
	if (err != SUCCESS)
	{
		return err;
	}
	return SUCCESS;
}

ErrorCode FileWriter::truncateFile(Ino inodeNumber, uint32_t newSize)
{
	MinixInode3 inode = {};
	ErrorCode err = inodeReader->readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (newSize == inode.i_size)
	{
		return SUCCESS;
	}
	if (newSize > inode.i_size)
	{
		while (inode.i_size < newSize)
		{
			uint32_t allocSize = std::min(static_cast<uint32_t>(newSize - inode.i_size), static_cast<uint32_t>(MAX_ALLOC_MEMORY_SIZE));
			uint8_t *zeroBuffer = static_cast<uint8_t *>(calloc(1, allocSize));
			if (zeroBuffer == nullptr)
			{
				return ERROR_CANNOT_ALLOCATE_MEMORY;
			}
			err = writeFile(inodeNumber, zeroBuffer, inode.i_size, allocSize);
			free(zeroBuffer);
			if (err != SUCCESS)
			{
				return err;
			}
			inode.i_size += allocSize;
		}
		return SUCCESS;
	}
	err = inodeWriter->writeInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	Zno lastZoneIndex = (newSize - 1) / layout->zoneSize;
	Zno oldLastZoneIndex = (inode.i_size - 1) / layout->zoneSize;
	for (Zno zoneIndex = newSize == 0 ? 0 : lastZoneIndex + 1; zoneIndex <= oldLastZoneIndex; zoneIndex++)
	{
		err = fileMapper->freeLogicalZone(inode, zoneIndex);
		if (err != SUCCESS)
		{
			return err;
		}
	}
	if (newSize % layout->zoneSize != 0 && lastZoneIndex <= oldLastZoneIndex)
	{
		uint32_t writeSize = layout->zoneSize - (newSize % layout->zoneSize);
		writeSize = std::min(writeSize, inode.i_size - newSize);
		uint8_t *zeroBuffer = static_cast<uint8_t *>(calloc(1, writeSize));
		if (zeroBuffer == nullptr)
		{
			return ERROR_CANNOT_ALLOCATE_MEMORY;
		}
		err = writeFile(inodeNumber, zeroBuffer, newSize, writeSize);
		free(zeroBuffer);
		if (err != SUCCESS)
		{
			return err;
		}
	}
	inode.i_size = newSize;
	return inodeWriter->writeInode(inodeNumber, &inode);
}