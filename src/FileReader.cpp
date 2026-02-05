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

ErrorCode FileReader::readFile(const MinixInode3 &inode, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	Layout &layout = *this->layout;
	uint32_t g_ZoneSize = layout.zoneSize;
	uint32_t g_IndirectZonesPerBlock = layout.indirectZonesPerBlock;
	ErrorCode err = SUCCESS;
	for (int i = 0; i < MINIX3_DIRECT_ZONES && sizeToRead > 0; i++)
	{
		if (offset >= g_ZoneSize)
		{
			offset -= g_ZoneSize;
			continue;
		}
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = std::min(g_ZoneSize - offset, sizeToRead);
		err = readOneZoneData(zoneNumber, buffer, toRead, offset);
		offset = 0;
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	for (int i = MINIX3_SINGLE_INDIRECT_ZONE_INDEX; i < MINIX3_SINGLE_INDIRECT_ZONE_INDEX + MINIX3_SINGLE_INDIRECT_ZONE_INDEX_COUNT && sizeToRead > 0; i++)
	{
		if (offset >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock)
		{
			offset -= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock;
			continue;
		}
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = std::min(static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock - offset, static_cast<uint64_t>(sizeToRead));
		err = readSingleIndirectData(zoneNumber, buffer, toRead, offset);
		offset = 0;
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	for (int i = MINIX3_DOUBLE_INDIRECT_ZONE_INDEX; i < MINIX3_DOUBLE_INDIRECT_ZONE_INDEX + MINIX3_DOUBLE_INDIRECT_ZONE_INDEX_COUNT && sizeToRead > 0; i++)
	{
		if (offset >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock)
		{
			offset -= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock;
			continue;
		}
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = std::min(static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock - offset, static_cast<uint64_t>(sizeToRead));
		err = readDoubleIndirectData(zoneNumber, buffer, toRead, offset);
		offset = 0;
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	for (int i = MINIX3_TRIPLE_INDIRECT_ZONE_INDEX; i < MINIX3_TRIPLE_INDIRECT_ZONE_INDEX + MINIX3_TRIPLE_INDIRECT_ZONE_INDEX_COUNT && sizeToRead > 0; i++)
	{
		if (offset >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock)
		{
			offset -= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock;
			continue;
		}
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = std::min(static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock - offset, static_cast<uint64_t>(sizeToRead));
		err = readTripleIndirectData(zoneNumber, buffer, toRead, offset);
		offset = 0;
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	if (sizeToRead > 0)
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}

ErrorCode FileReader::readOneZoneData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	Layout &layout = *this->layout;
	Bno blockNumber = layout.zone2Block(zoneNumber);
	uint32_t g_BlockSize = layout.blockSize;
	uint32_t g_BlocksPerZone = layout.blocksPerZone;
	for (uint32_t i = 0; i < g_BlocksPerZone; i++)
	{
		if (offset >= g_BlockSize)
		{
			offset -= g_BlockSize;
			continue;
		}
		if (offset == 0 && sizeToRead >= g_BlockSize)
		{
			ErrorCode err = this->blockDevice->readBlock(blockNumber + i, buffer);
			if (err != SUCCESS)
			{
				return err;
			}
			buffer += g_BlockSize;
			sizeToRead -= g_BlockSize;
		}
		else
		{
			uint32_t toRead = std::min(g_BlockSize - offset, sizeToRead);
			uint8_t *tempBuffer = static_cast<uint8_t*>(malloc(g_BlockSize));
			if (tempBuffer == nullptr)
			{
				return ERROR_CANNOT_ALLOCATE_MEMORY;
			}
			ErrorCode err = this->blockDevice->readBlock(blockNumber + i, tempBuffer);
			if (err != SUCCESS)
			{
				free(tempBuffer);
				return err;
			}
			memcpy(buffer, tempBuffer + offset, toRead);
			free(tempBuffer);
			sizeToRead -= toRead;
			buffer += toRead;
			offset = 0;
		}
	}
	if (sizeToRead > 0)
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}

ErrorCode FileReader::readSingleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	Layout &layout = *this->layout;
	uint32_t g_ZoneSize = layout.zoneSize;
	uint32_t g_IndirectZonesPerBlock = layout.indirectZonesPerBlock;
	IndirectBlock indirectBlock;
	ErrorCode err = this->blockDevice->readBlock(layout.zone2Block(zoneNumber), &indirectBlock);
	if (err != SUCCESS)
	{
		return err;
	}
	for (uint32_t i = 0; i < g_IndirectZonesPerBlock && sizeToRead > 0; i++)
	{
		if (offset >= g_ZoneSize)
		{
			offset -= g_ZoneSize;
			continue;
		}
		Zno dataZoneNumber = indirectBlock.zones[i];
		if (dataZoneNumber == 0)
		{
			return ERROR_FS_BROKEN;
		}
		uint32_t toRead = std::min(g_ZoneSize - offset, sizeToRead);
		err = readOneZoneData(dataZoneNumber, buffer, toRead, offset);
		offset = 0;
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	if (sizeToRead > 0)
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}

ErrorCode FileReader::readDoubleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	Layout &layout = *this->layout;
	uint32_t g_ZoneSize = layout.zoneSize;
	uint32_t g_IndirectZonesPerBlock = layout.indirectZonesPerBlock;
	IndirectBlock indirectBlock;
	ErrorCode err = this->blockDevice->readBlock(layout.zone2Block(zoneNumber), &indirectBlock);
	if (err != SUCCESS)
	{
		return err;
	}
	for (uint32_t i = 0; i < g_IndirectZonesPerBlock && sizeToRead > 0; i++)
	{
		if (offset >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock)
		{
			offset -= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock;
			continue;
		}
		Zno dataZoneNumber = indirectBlock.zones[i];
		if (dataZoneNumber == 0)
		{
			return ERROR_FS_BROKEN;
		}
		uint32_t toRead = std::min(static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock - offset, static_cast<uint64_t>(sizeToRead));
		err = readSingleIndirectData(dataZoneNumber, buffer, toRead, offset);
		offset = 0;
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	if (sizeToRead > 0)
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}

ErrorCode FileReader::readTripleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	Layout &layout = *this->layout;
	uint32_t g_ZoneSize = layout.zoneSize;
	uint32_t g_IndirectZonesPerBlock = layout.indirectZonesPerBlock;
	IndirectBlock indirectBlock;
	ErrorCode err = this->blockDevice->readBlock(layout.zone2Block(zoneNumber), &indirectBlock);
	if (err != SUCCESS)
	{
		return err;
	}
	for (uint32_t i = 0; i < g_IndirectZonesPerBlock && sizeToRead > 0; i++)
	{
		if (offset >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock)
		{
			offset -= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock;
			continue;
		}
		Zno dataZoneNumber = indirectBlock.zones[i];
		if (dataZoneNumber == 0)
		{
			return ERROR_FS_BROKEN;
		}
		uint32_t toRead = std::min(static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock - offset, static_cast<uint64_t>(sizeToRead));
		err = readDoubleIndirectData(dataZoneNumber, buffer, toRead, offset);
		offset = 0;
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	if (sizeToRead > 0)
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}