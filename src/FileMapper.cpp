#include "FileMapper.h"
#include "Constants.h"
#include "IndirectBlock.h"

void FileMapper::setBlockDevice(BlockDevice &blockDevice)
{
	this->blockDevice = &blockDevice;
}

void FileMapper::setInodeReader(InodeReader &inodeReader)
{
	this->inodeReader = &inodeReader;
}

void FileMapper::setZonesPerIndirectBlock(uint32_t zonesPerIndirectBlock)
{
	this->zonesPerIndirectBlock = zonesPerIndirectBlock;
}

void FileMapper::setBlocksPerZone(uint32_t blocksPerZone)
{
	this->blocksPerZone = blocksPerZone;
}

ErrorCode FileMapper::mapLogicalToPhysical(const MinixInode3 &inode, Zno logicalZoneIndex, Zno &outPhysicalZoneIndex)
{
	if (logicalZoneIndex < MINIX3_DIRECT_ZONES)
	{
		outPhysicalZoneIndex = inode.i_zone[logicalZoneIndex];
		return SUCCESS;
	}
	logicalZoneIndex -= MINIX3_DIRECT_ZONES;
	uint32_t indirectZoneIndex = logicalZoneIndex / zonesPerIndirectBlock;
	if (indirectZoneIndex < MINIX3_SINGLE_INDIRECT_ZONE_INDEX_COUNT)
	{
		if (inode.i_zone[MINIX3_SINGLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] == 0)
		{
			outPhysicalZoneIndex = 0;
			return SUCCESS;
		}
		IndirectBlock singleIndirectBlock;
		ErrorCode err = blockDevice->readBlock(inode.i_zone[MINIX3_SINGLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] * blocksPerZone , &singleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		outPhysicalZoneIndex = singleIndirectBlock.zones[logicalZoneIndex % zonesPerIndirectBlock];
		return SUCCESS;
	}
	logicalZoneIndex -= MINIX3_SINGLE_INDIRECT_ZONE_INDEX_COUNT * zonesPerIndirectBlock;
	indirectZoneIndex = logicalZoneIndex / (zonesPerIndirectBlock * zonesPerIndirectBlock);
	if (indirectZoneIndex < MINIX3_DOUBLE_INDIRECT_ZONE_INDEX_COUNT)
	{
		if (inode.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] == 0)
		{
			outPhysicalZoneIndex = 0;
			return SUCCESS;
		}
		IndirectBlock doubleIndirectBlock;
		ErrorCode err = blockDevice->readBlock(inode.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] * blocksPerZone, &doubleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t singleIndirectZoneIndex = logicalZoneIndex / zonesPerIndirectBlock % zonesPerIndirectBlock;
		if (doubleIndirectBlock.zones[singleIndirectZoneIndex] == 0)
		{
			outPhysicalZoneIndex = 0;
			return SUCCESS;
		}
		IndirectBlock singleIndirectBlock;
		err = blockDevice->readBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex] * blocksPerZone, &singleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		outPhysicalZoneIndex = singleIndirectBlock.zones[logicalZoneIndex % zonesPerIndirectBlock];
		return SUCCESS;
	}
	logicalZoneIndex -= MINIX3_DOUBLE_INDIRECT_ZONE_INDEX_COUNT * zonesPerIndirectBlock * zonesPerIndirectBlock;
	indirectZoneIndex = logicalZoneIndex / (zonesPerIndirectBlock * zonesPerIndirectBlock * zonesPerIndirectBlock);
	if (indirectZoneIndex < MINIX3_TRIPLE_INDIRECT_ZONE_INDEX_COUNT)
	{
		if (inode.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] == 0)
		{
			outPhysicalZoneIndex = 0;
			return SUCCESS;
		}
		IndirectBlock tripleIndirectBlock;
		ErrorCode err = blockDevice->readBlock(inode.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] * blocksPerZone, &tripleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t doubleIndirectZoneIndex = logicalZoneIndex / (zonesPerIndirectBlock * zonesPerIndirectBlock) % zonesPerIndirectBlock;
		if (tripleIndirectBlock.zones[doubleIndirectZoneIndex] == 0)
		{
			outPhysicalZoneIndex = 0;
			return SUCCESS;
		}
		IndirectBlock doubleIndirectBlock;
		err = blockDevice->readBlock(tripleIndirectBlock.zones[doubleIndirectZoneIndex] * blocksPerZone, &doubleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t singleIndirectZoneIndex = logicalZoneIndex / zonesPerIndirectBlock % zonesPerIndirectBlock;
		if (doubleIndirectBlock.zones[singleIndirectZoneIndex] == 0)
		{
			outPhysicalZoneIndex = 0;
			return SUCCESS;
		}
		IndirectBlock singleIndirectBlock;
		err = blockDevice->readBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex] * blocksPerZone, &singleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		outPhysicalZoneIndex = singleIndirectBlock.zones[logicalZoneIndex % zonesPerIndirectBlock];
		return SUCCESS;
	}
	return ERROR_INVALID_FILE_OFFSET;
}