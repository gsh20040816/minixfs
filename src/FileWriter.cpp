#include "FileWriter.h"

void FileWriter::setBlockDevice(BlockDevice &blockDevice)
{
	this->blockDevice = &blockDevice;
}

void FileWriter::setZmapAllocator(Allocator &zmapAllocator)
{
	this->zmapAllocator = &zmapAllocator;
}

void FileWriter::setFileMapper(FileMapper &fileMapper)
{
	this->fileMapper = &fileMapper;
}

void FileWriter::setLayout(Layout &layout)
{
	this->layout = &layout;
}

ErrorCode FileWriter::writeFile(const MinixInode3 &inode, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite)
{
	if (sizeToWrite == 0)
	{
		return SUCCESS;
	}
	MinixInode3 inodeForMap = inode;
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
	}
	return SUCCESS;
}
