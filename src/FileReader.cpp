#include "FileReader.h"
#include "IndirectBlock.h"
#include <cstring>

void FileReader::setBlockDevice(BlockDevice &blockDevice)
{
	this->blockDevice = &blockDevice;
}

void FileReader::setLayout(Layout &layout)
{
	this->layout = &layout;
}

void FileReader::setFileMapper(FileMapper &fileMapper)
{
	this->fileMapper = &fileMapper;
}

ErrorCode FileReader::readFile(const MinixInode3 &inode, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	if (offset >= inode.i_size)
	{
		return ERROR_INVALID_FILE_OFFSET;
	}
	if (sizeToRead == 0)
	{
		return SUCCESS;
	}
	if (offset + sizeToRead > inode.i_size)
	{
		return ERROR_INVALID_FILE_OFFSET;
	}
	MinixInode3 inodeForMap = inode;
	Zno startZoneIndex = offset / layout->zoneSize;
	Zno endZoneIndex = (offset + sizeToRead - 1) / layout->zoneSize;
	for (Zno zoneIndex = startZoneIndex; zoneIndex <= endZoneIndex; zoneIndex++)
	{
		Zno physicalZoneIndex;
		ErrorCode err = fileMapper->mapLogicalToPhysical(inodeForMap, zoneIndex, physicalZoneIndex);
		if (err != SUCCESS)
		{
			return err;
		}
		if (physicalZoneIndex == 0)
		{
			if (inode.isRegularFile())
			{
				uint32_t readSize = zoneIndex == startZoneIndex ? std::min(sizeToRead, layout->zoneSize - (offset % layout->zoneSize)) : zoneIndex == endZoneIndex ? (offset + sizeToRead - 1) % layout->zoneSize + 1 : layout->zoneSize;
				memset(zoneIndex == startZoneIndex ? buffer : buffer + (zoneIndex - startZoneIndex) * layout->zoneSize - (offset % layout->zoneSize), 0, readSize);
				continue;
			}
			else
			{
				return ERROR_FS_BROKEN;
			}
		}
		if (zoneIndex == startZoneIndex)
		{
			uint8_t *zoneBuffer = static_cast<uint8_t *>(malloc(layout->zoneSize));
			if (!zoneBuffer)
			{
				return ERROR_CANNOT_ALLOCATE_MEMORY;
			}
			err = blockDevice->readZone(physicalZoneIndex, zoneBuffer);
			if (err != SUCCESS)
			{
				free(zoneBuffer);
				return err;
			}
			memcpy(buffer, zoneBuffer + (offset % layout->zoneSize), std::min(sizeToRead, layout->zoneSize - (offset % layout->zoneSize)));
			free(zoneBuffer);
		}
		else if (zoneIndex == endZoneIndex)
		{
			uint8_t *zoneBuffer = static_cast<uint8_t *>(malloc(layout->zoneSize));
			if (!zoneBuffer)
			{
				return ERROR_CANNOT_ALLOCATE_MEMORY;
			}
			err = blockDevice->readZone(physicalZoneIndex, zoneBuffer);
			if (err != SUCCESS)
			{
				free(zoneBuffer);
				return err;
			}
			memcpy(buffer + (zoneIndex - startZoneIndex) * layout->zoneSize - (offset % layout->zoneSize), zoneBuffer, (offset + sizeToRead - 1) % layout->zoneSize + 1);
			free(zoneBuffer);
		}
		else
		{
			err = blockDevice->readZone(physicalZoneIndex, buffer + (zoneIndex - startZoneIndex) * layout->zoneSize - (offset % layout->zoneSize));
			if (err != SUCCESS)
			{
				return err;
			}
		}
	}
	return SUCCESS;
}