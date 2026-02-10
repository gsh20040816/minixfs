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
	if (!inodeForMap.isRegularFile() && !inodeForMap.isDirectory())
	{
		return ERROR_NOT_REGULAR_FILE;
	}

	err = blockDevice->startTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	err = fileMapper->zmapAllocator->beginTransaction();
	if (err != SUCCESS)
	{
		blockDevice->revertTransaction();
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
			fileMapper->zmapAllocator->revertTransaction();
			blockDevice->revertTransaction();
			return err;
		}
		uint32_t writeSize = zoneIndex == startZoneIndex ? std::min(sizeToWrite, layout->zoneSize - (offset % layout->zoneSize)) : zoneIndex == endZoneIndex ? (offset + sizeToWrite - 1) % layout->zoneSize + 1 : layout->zoneSize;
		if (writeSize == layout->zoneSize)
		{
			err = blockDevice->writeZone(physicalZoneIndex, data + (zoneIndex - startZoneIndex) * layout->zoneSize - (offset % layout->zoneSize));
			if (err != SUCCESS)
			{
				fileMapper->zmapAllocator->revertTransaction();
				blockDevice->revertTransaction();
				return err;
			}
		}
		else
		{
			uint8_t *zoneBuffer = static_cast<uint8_t *>(malloc(layout->zoneSize));
			if (zoneBuffer == nullptr)
			{
				fileMapper->zmapAllocator->revertTransaction();
				blockDevice->revertTransaction();
				return ERROR_CANNOT_ALLOCATE_MEMORY;
			}
			err = blockDevice->readZone(physicalZoneIndex, zoneBuffer);
			if (err != SUCCESS)
			{
				fileMapper->zmapAllocator->revertTransaction();
				blockDevice->revertTransaction();
				free(zoneBuffer);
				return err;
			}
			memcpy(zoneBuffer + (zoneIndex == startZoneIndex ? (offset % layout->zoneSize) : 0), zoneIndex == startZoneIndex ? data : data + (zoneIndex - startZoneIndex) * layout->zoneSize - (offset % layout->zoneSize), writeSize);
			err = blockDevice->writeZone(physicalZoneIndex, zoneBuffer);
			free(zoneBuffer);
			if (err != SUCCESS)
			{
				fileMapper->zmapAllocator->revertTransaction();
				blockDevice->revertTransaction();
				return err;
			}
		}
	}
	inodeForMap.i_size = std::max(inodeForMap.i_size, offset + sizeToWrite);
	err = inodeWriter->writeInode(inodeNumber, &inodeForMap);
	if (err != SUCCESS)
	{
		fileMapper->zmapAllocator->revertTransaction();
		blockDevice->revertTransaction();
		return err;
	}
	fileMapper->zmapAllocator->commitTransaction();
	blockDevice->commitTransaction();
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
	if (!inode.isRegularFile())
	{
		return ERROR_NOT_REGULAR_FILE;
	}
	if (newSize == inode.i_size)
	{
		return SUCCESS;
	}
	if (newSize > inode.i_size)
	{
		uint8_t *zeroBuffer = static_cast<uint8_t *>(calloc(1, newSize - inode.i_size));
		if (zeroBuffer == nullptr)
		{
			return ERROR_CANNOT_ALLOCATE_MEMORY;
		}
		err = writeFile(inodeNumber, zeroBuffer, inode.i_size, newSize - inode.i_size);
		free(zeroBuffer);
		return err;
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
	inode.i_size = newSize;
	return inodeWriter->writeInode(inodeNumber, &inode);
}