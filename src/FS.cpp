#include "FS.h"
#include "Constants.h"
#include "Utils.h"
#include "Inode.h"
#include "IndirectBlock.h"
#include "DirEntry.h"
#include <cstring>

FS::FS(const std::string &devicePath): g_BlockDevice(devicePath), g_Superblock(), g_BlockSize(0) {}

ErrorCode FS::mount()
{
	BlockDevice &bd = g_BlockDevice;
	ErrorCode err = bd.open();
	if (err != SUCCESS)
	{
		return err;
	}

	err = bd.readBytes(MINIX3_SUPERBLOCK_OFFSET, &g_Superblock, sizeof(MinixSuperblock3));
	if (err != SUCCESS)
	{
		bd.close();
		return err;
	}

	MinixSuperblock3 &sb = g_Superblock;
	if (sb.s_magic != MINIX3_MAGIC)
	{
		bd.close();
		return ERROR_INVALID_SUPERBLOCK;
	}

	g_BlockSize = g_Superblock.s_blocksize;
	bd.setBlockSize(g_BlockSize);
	g_InodesBitmapStart = MINIX3_IZONE_START_BLOCK;
	g_ZonesBitmapStart = g_InodesBitmapStart + sb.s_imap_blocks;
	g_InodesTableStart = g_ZonesBitmapStart + sb.s_zmap_blocks;
	g_InodesPerBlock = g_BlockSize / MINIX3_INODE_SIZE;
	g_DataZonesStart = g_InodesTableStart + ((sb.s_ninodes * MINIX3_INODE_SIZE + g_BlockSize - 1) / g_BlockSize);
	if (g_DataZonesStart != sb.s_firstdatazone)
	{
		bd.close();
		return ERROR_INVALID_SUPERBLOCK;
	}
	g_BlocksPerZone = 1 << sb.s_log_zone_size;
	g_ZoneSize = g_BlockSize * g_BlocksPerZone;
	g_IndirectZonesPerBlock = g_BlockSize / sizeof(uint32_t);
	return SUCCESS;
}

ErrorCode FS::unmount()
{
	return g_BlockDevice.close();
}

ErrorCode FS::readInode(Ino inodeNumber, void* buffer)
{
	Bno inodeBlockNumber = g_InodesTableStart + inodeNumber / g_InodesPerBlock;
	uint32_t inodeOffset = (inodeNumber % g_InodesPerBlock) * MINIX3_INODE_SIZE;
	void* blockBuffer = malloc(g_BlockSize);
	if (blockBuffer == nullptr)
	{
		return ERROR_CANNOT_ALLOCATE_MEMORY;
	}
	ErrorCode err = g_BlockDevice.readBlock(inodeBlockNumber, blockBuffer);
	if (err != SUCCESS)
	{
		free(blockBuffer);
		return err;
	}
	memcpy(buffer, static_cast<uint8_t*>(blockBuffer) + inodeOffset, MINIX3_INODE_SIZE);
	free(blockBuffer);
	return SUCCESS;
}

ErrorCode FS::readOneZoneData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead)
{
	Bno blockNumber = g_DataZonesStart + zoneNumber * g_BlocksPerZone;
	for (uint32_t i = 0; i < g_BlocksPerZone; i++)
	{
		if (sizeToRead >= g_BlockSize)
		{
			ErrorCode err = g_BlockDevice.readBlock(blockNumber + i, buffer);
			if (err != SUCCESS)
			{
				return err;
			}
			buffer += g_BlockSize;
			sizeToRead -= g_BlockSize;
		}
		else
		{
			uint8_t *tempBuffer = static_cast<uint8_t*>(malloc(g_BlockSize));
			if (tempBuffer == nullptr)
			{
				return ERROR_CANNOT_ALLOCATE_MEMORY;
			}
			ErrorCode err = g_BlockDevice.readBlock(blockNumber + i, tempBuffer);
			if (err != SUCCESS)
			{
				free(tempBuffer);
				return err;
			}
			memcpy(buffer, tempBuffer, sizeToRead);
			free(tempBuffer);
			sizeToRead = 0;
			return SUCCESS;
		}
	}
	if (sizeToRead > 0)
	{
		return ERROR_READ_FAIL;
	}
	return SUCCESS;
}

ErrorCode FS::readSingleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead)
{
	IndirectBlock indirectBlock;
	ErrorCode err = g_BlockDevice.readBlock(g_DataZonesStart + zoneNumber * g_BlocksPerZone, &indirectBlock);
	if (err != SUCCESS)
	{
		return err;
	}
	for (uint32_t i = 0; i < g_IndirectZonesPerBlock && sizeToRead > 0; i++)
	{
		Zno dataZoneNumber = indirectBlock.zones[i];
		if (dataZoneNumber == 0)
		{
			return ERROR_FS_BROKEN;
		}
		uint32_t toRead = (sizeToRead >= g_ZoneSize) ? g_ZoneSize : sizeToRead;
		err = readOneZoneData(dataZoneNumber, buffer, toRead);
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

ErrorCode FS::readDoubleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead)
{
	IndirectBlock indirectBlock;
	ErrorCode err = g_BlockDevice.readBlock(g_DataZonesStart + zoneNumber * g_BlocksPerZone, &indirectBlock);
	if (err != SUCCESS)
	{
		return err;
	}
	for (uint32_t i = 0; i < g_IndirectZonesPerBlock && sizeToRead > 0; i++)
	{
		Zno dataZoneNumber = indirectBlock.zones[i];
		if (dataZoneNumber == 0)
		{
			return ERROR_FS_BROKEN;
		}
		uint32_t toRead = (sizeToRead >= g_ZoneSize * g_IndirectZonesPerBlock) ? g_ZoneSize * g_IndirectZonesPerBlock : sizeToRead;
		err = readSingleIndirectData(dataZoneNumber, buffer, toRead);
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

ErrorCode FS::readTripleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead)
{
	IndirectBlock indirectBlock;
	ErrorCode err = g_BlockDevice.readBlock(g_DataZonesStart + zoneNumber * g_BlocksPerZone, &indirectBlock);
	if (err != SUCCESS)
	{
		return err;
	}
	for (uint32_t i = 0; i < g_IndirectZonesPerBlock && sizeToRead > 0; i++)
	{
		Zno dataZoneNumber = indirectBlock.zones[i];
		if (dataZoneNumber == 0)
		{
			return ERROR_FS_BROKEN;
		}
		uint32_t toRead = (sizeToRead >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock) ? g_ZoneSize * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock : sizeToRead;
		err = readDoubleIndirectData(dataZoneNumber, buffer, toRead);
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

ErrorCode FS::readInodeFullData(Ino inodeNumber, uint8_t *buffer)
{
	MinixInode3 inode;
	ErrorCode err = readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}

	uint32_t sizeToRead = inode.i_size;
	for (int i = 0; i < MINIX3_DIRECT_ZONES && sizeToRead > 0; i++)
	{
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = (sizeToRead >= g_ZoneSize) ? g_ZoneSize : sizeToRead;
		err = readOneZoneData(zoneNumber, buffer, toRead);
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	for (int i = MINIX3_SINGLE_INDIRECT_ZONE_INDEX; i < MINIX3_SINGLE_INDIRECT_ZONE_INDEX + MINIX3_SINGLE_INDIRECT_ZONE_INDEX_COUNT && sizeToRead > 0; i++)
	{
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = (sizeToRead >= g_ZoneSize * g_IndirectZonesPerBlock) ? g_ZoneSize * g_IndirectZonesPerBlock : sizeToRead;
		err = readSingleIndirectData(zoneNumber, buffer, toRead);
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	for (int i = MINIX3_DOUBLE_INDIRECT_ZONE_INDEX; i < MINIX3_DOUBLE_INDIRECT_ZONE_INDEX + MINIX3_DOUBLE_INDIRECT_ZONE_INDEX_COUNT && sizeToRead > 0; i++)
	{
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = (sizeToRead >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock) ? g_ZoneSize * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock : sizeToRead;
		err = readDoubleIndirectData(zoneNumber, buffer, toRead);
		if (err != SUCCESS)
		{
			return err;
		}
		sizeToRead -= toRead;
		buffer += toRead;
	}
	for (int i = MINIX3_TRIPLE_INDIRECT_ZONE_INDEX; i < MINIX3_TRIPLE_INDIRECT_ZONE_INDEX + MINIX3_TRIPLE_INDIRECT_ZONE_INDEX_COUNT && sizeToRead > 0; i++)
	{
		if (inode.i_zone[i] == 0)
		{
			return ERROR_FS_BROKEN;
		}
		Zno zoneNumber = inode.i_zone[i];
		uint32_t toRead = (sizeToRead >= static_cast<uint64_t>(g_ZoneSize) * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock) ? g_ZoneSize * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock * g_IndirectZonesPerBlock : sizeToRead;
		err = readTripleIndirectData(zoneNumber, buffer, toRead);
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

Ino FS::getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name)
{
	MinixInode3 parentInode;
	ErrorCode err = readInode(parentInodeNumber, &parentInode);
	if (err != SUCCESS)
	{
		return 0;
	}
	if (!parentInode.isDirectory())
	{
		return 0;
	}
	uint32_t dirSize = parentInode.i_size;
	uint8_t *dirData = static_cast<uint8_t*>(malloc(dirSize));
	if (dirData == nullptr)
	{
		return 0;
	}
	err = readInodeFullData(parentInodeNumber, dirData);
	if (err != SUCCESS)
	{
		free(dirData);
		return 0;
	}
	for (uint32_t offset = 0; offset < dirSize; offset += sizeof(DirEntryOnDisk))
	{
		DirEntryOnDisk *entry = reinterpret_cast<DirEntryOnDisk*>(dirData + offset);
		std::string entryName(entry->d_name);
		if (entryName == name && entry->d_inode != 0)
		{
			Ino inodeNumber = entry->d_inode;
			free(dirData);
			return inodeNumber;
		}
	}
	free(dirData);
	return 0;
}

Ino FS::getInodeFromPath(const std::string &path)
{
	Ino currentInode = MINIX3_ROOT_INODE;
	std::vector<std::string> components = splitPath(path);
	for (const std::string &component: components)
	{
		currentInode = getInodeFromParentAndName(currentInode, component);
		if (currentInode == 0)
		{
			return 0;
		}
	}
	return currentInode;
}