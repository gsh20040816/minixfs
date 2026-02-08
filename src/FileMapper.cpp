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

void FileMapper::setZmapAllocator(Allocator &zmapAllocator)
{
	this->zmapAllocator = &zmapAllocator;
}

ErrorCode FileMapper::mapLogicalToPhysical(MinixInode3 &inode, Zno logicalZoneIndex, Zno &outPhysicalZoneIndex, bool allocateIfNotMapped)
{
	if (blockDevice == nullptr || zonesPerIndirectBlock == 0 || blocksPerZone == 0)
	{
		return ERROR_FS_BROKEN;
	}
	if (allocateIfNotMapped && zmapAllocator == nullptr)
	{
		return ERROR_FS_BROKEN;
	}
	auto allocateZone = [&](Zno &outZone) -> ErrorCode
	{
		ErrorCode err = SUCCESS;
		outZone = zmapAllocator->allocateBmap(err);
		return err;
	};
	auto initIndirectBlock = [&](Zno zoneNumber) -> ErrorCode
	{
		IndirectBlock emptyBlock{};
		return blockDevice->writeBlock(zoneNumber * blocksPerZone, &emptyBlock);
	};

	if (logicalZoneIndex < MINIX3_DIRECT_ZONES)
	{
		if (allocateIfNotMapped && inode.i_zone[logicalZoneIndex] == 0)
		{
			Zno newZone = 0;
			ErrorCode err = allocateZone(newZone);
			if (err != SUCCESS)
			{
				return err;
			}
			inode.i_zone[logicalZoneIndex] = newZone;
		}
		outPhysicalZoneIndex = inode.i_zone[logicalZoneIndex];
		return SUCCESS;
	}
	logicalZoneIndex -= MINIX3_DIRECT_ZONES;
	uint32_t indirectZoneIndex = logicalZoneIndex / zonesPerIndirectBlock;
	if (indirectZoneIndex < MINIX3_SINGLE_INDIRECT_ZONE_INDEX_COUNT)
	{
		Zno singleIndirectZone = inode.i_zone[MINIX3_SINGLE_INDIRECT_ZONE_INDEX + indirectZoneIndex];
		if (singleIndirectZone == 0)
		{
			if (allocateIfNotMapped)
			{
				ErrorCode err = allocateZone(singleIndirectZone);
				if (err != SUCCESS)
				{
					return err;
				}
				err = initIndirectBlock(singleIndirectZone);
				if (err != SUCCESS)
				{
					return err;
				}
				inode.i_zone[MINIX3_SINGLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] = singleIndirectZone;
			}
			else
			{
				outPhysicalZoneIndex = 0;
				return SUCCESS;
			}
		}
		IndirectBlock singleIndirectBlock;
		ErrorCode err = blockDevice->readBlock(singleIndirectZone * blocksPerZone, &singleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t dataZoneIndex = logicalZoneIndex % zonesPerIndirectBlock;
		if (allocateIfNotMapped && singleIndirectBlock.zones[dataZoneIndex] == 0)
		{
			err = allocateZone(singleIndirectBlock.zones[dataZoneIndex]);
			if (err != SUCCESS)
			{
				return err;
			}
			err = blockDevice->writeBlock(singleIndirectZone * blocksPerZone, &singleIndirectBlock);
			if (err != SUCCESS)
			{
				return err;
			}
		}
		outPhysicalZoneIndex = singleIndirectBlock.zones[dataZoneIndex];
		return SUCCESS;
	}
	logicalZoneIndex -= MINIX3_SINGLE_INDIRECT_ZONE_INDEX_COUNT * zonesPerIndirectBlock;
	indirectZoneIndex = logicalZoneIndex / (zonesPerIndirectBlock * zonesPerIndirectBlock);
	if (indirectZoneIndex < MINIX3_DOUBLE_INDIRECT_ZONE_INDEX_COUNT)
	{
		Zno doubleIndirectZone = inode.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE_INDEX + indirectZoneIndex];
		if (doubleIndirectZone == 0)
		{
			if (allocateIfNotMapped)
			{
				ErrorCode err = allocateZone(doubleIndirectZone);
				if (err != SUCCESS)
				{
					return err;
				}
				err = initIndirectBlock(doubleIndirectZone);
				if (err != SUCCESS)
				{
					return err;
				}
				inode.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] = doubleIndirectZone;
			}
			else
			{
				outPhysicalZoneIndex = 0;
				return SUCCESS;
			}
		}
		IndirectBlock doubleIndirectBlock;
		ErrorCode err = blockDevice->readBlock(doubleIndirectZone * blocksPerZone, &doubleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t singleIndirectZoneIndex = logicalZoneIndex / zonesPerIndirectBlock % zonesPerIndirectBlock;
		if (doubleIndirectBlock.zones[singleIndirectZoneIndex] == 0)
		{
			if (allocateIfNotMapped)
			{
				err = allocateZone(doubleIndirectBlock.zones[singleIndirectZoneIndex]);
				if (err != SUCCESS)
				{
					return err;
				}
				err = initIndirectBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex]);
				if (err != SUCCESS)
				{
					return err;
				}
				err = blockDevice->writeBlock(doubleIndirectZone * blocksPerZone, &doubleIndirectBlock);
				if (err != SUCCESS)
				{
					return err;
				}
			}
			else
			{
				outPhysicalZoneIndex = 0;
				return SUCCESS;
			}
		}
		IndirectBlock singleIndirectBlock;
		err = blockDevice->readBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex] * blocksPerZone, &singleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t dataZoneIndex = logicalZoneIndex % zonesPerIndirectBlock;
		if (allocateIfNotMapped && singleIndirectBlock.zones[dataZoneIndex] == 0)
		{
			err = allocateZone(singleIndirectBlock.zones[dataZoneIndex]);
			if (err != SUCCESS)
			{
				return err;
			}
			err = blockDevice->writeBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex] * blocksPerZone, &singleIndirectBlock);
			if (err != SUCCESS)
			{
				return err;
			}
		}
		outPhysicalZoneIndex = singleIndirectBlock.zones[dataZoneIndex];
		return SUCCESS;
	}
	logicalZoneIndex -= MINIX3_DOUBLE_INDIRECT_ZONE_INDEX_COUNT * zonesPerIndirectBlock * zonesPerIndirectBlock;
	indirectZoneIndex = logicalZoneIndex / (zonesPerIndirectBlock * zonesPerIndirectBlock * zonesPerIndirectBlock);
	if (indirectZoneIndex < MINIX3_TRIPLE_INDIRECT_ZONE_INDEX_COUNT)
	{
		Zno tripleIndirectZone = inode.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE_INDEX + indirectZoneIndex];
		if (tripleIndirectZone == 0)
		{
			if (allocateIfNotMapped)
			{
				ErrorCode err = allocateZone(tripleIndirectZone);
				if (err != SUCCESS)
				{
					return err;
				}
				err = initIndirectBlock(tripleIndirectZone);
				if (err != SUCCESS)
				{
					return err;
				}
				inode.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE_INDEX + indirectZoneIndex] = tripleIndirectZone;
			}
			else
			{
				outPhysicalZoneIndex = 0;
				return SUCCESS;
			}
		}
		IndirectBlock tripleIndirectBlock;
		ErrorCode err = blockDevice->readBlock(tripleIndirectZone * blocksPerZone, &tripleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t doubleIndirectZoneIndex = logicalZoneIndex / (zonesPerIndirectBlock * zonesPerIndirectBlock) % zonesPerIndirectBlock;
		if (tripleIndirectBlock.zones[doubleIndirectZoneIndex] == 0)
		{
			if (allocateIfNotMapped)
			{
				err = allocateZone(tripleIndirectBlock.zones[doubleIndirectZoneIndex]);
				if (err != SUCCESS)
				{
					return err;
				}
				err = initIndirectBlock(tripleIndirectBlock.zones[doubleIndirectZoneIndex]);
				if (err != SUCCESS)
				{
					return err;
				}
				err = blockDevice->writeBlock(tripleIndirectZone * blocksPerZone, &tripleIndirectBlock);
				if (err != SUCCESS)
				{
					return err;
				}
			}
			else
			{
				outPhysicalZoneIndex = 0;
				return SUCCESS;
			}
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
			if (allocateIfNotMapped)
			{
				err = allocateZone(doubleIndirectBlock.zones[singleIndirectZoneIndex]);
				if (err != SUCCESS)
				{
					return err;
				}
				err = initIndirectBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex]);
				if (err != SUCCESS)
				{
					return err;
				}
				err = blockDevice->writeBlock(tripleIndirectBlock.zones[doubleIndirectZoneIndex] * blocksPerZone, &doubleIndirectBlock);
				if (err != SUCCESS)
				{
					return err;
				}
			}
			else
			{
				outPhysicalZoneIndex = 0;
				return SUCCESS;
			}
		}
		IndirectBlock singleIndirectBlock;
		err = blockDevice->readBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex] * blocksPerZone, &singleIndirectBlock);
		if (err != SUCCESS)
		{
			return err;
		}
		uint32_t dataZoneIndex = logicalZoneIndex % zonesPerIndirectBlock;
		if (singleIndirectBlock.zones[dataZoneIndex] == 0)
		{
			if (allocateIfNotMapped)
			{
				err = allocateZone(singleIndirectBlock.zones[dataZoneIndex]);
				if (err != SUCCESS)
				{
					return err;
				}
				err = blockDevice->writeBlock(doubleIndirectBlock.zones[singleIndirectZoneIndex] * blocksPerZone, &singleIndirectBlock);
				if (err != SUCCESS)
				{
					return err;
				}
			}
			else
			{
				outPhysicalZoneIndex = 0;
				return SUCCESS;
			}
		}
		outPhysicalZoneIndex = singleIndirectBlock.zones[dataZoneIndex];
		return SUCCESS;
	}
	return ERROR_INVALID_FILE_OFFSET;
}