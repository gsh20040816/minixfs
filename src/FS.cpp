#include "FS.h"
#include "Constants.h"
#include "Utils.h"
#include "Inode.h"
#include "IndirectBlock.h"
#include "DirEntry.h"
#include <cstring>

FS::FS(): g_BlockDevice(), g_Superblock(), g_BlockSize(0) {}

FS::FS(const std::string &devicePath): g_BlockDevice(devicePath), g_Superblock(), g_BlockSize(0) {}

void FS::setDevicePath(const std::string &devicePath)
{
	g_BlockDevice.setDevicePath(devicePath);
}

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
	if (g_BlockSize != 1024 && g_BlockSize != 2048 && g_BlockSize != 4096)
	{
		bd.close();
		return ERROR_FS_BROKEN;
	}
	bd.setBlockSize(g_BlockSize);
	g_InodesBitmapStart = MINIX3_IZONE_START_BLOCK;
	g_ZonesBitmapStart = g_InodesBitmapStart + sb.s_imap_blocks;
	g_InodesTableStart = g_ZonesBitmapStart + sb.s_zmap_blocks;
	g_InodesPerBlock = g_BlockSize / MINIX3_INODE_SIZE;
	g_DataZonesStart = g_InodesTableStart + ((sb.s_ninodes * MINIX3_INODE_SIZE + g_BlockSize - 1) / g_BlockSize);
	if (g_DataZonesStart != sb.s_firstdatazone)
	{
		bd.close();
		return ERROR_FS_BROKEN;
	}
	if (sb.s_log_zone_size > 7)
	{
		bd.close();
		return ERROR_FS_BROKEN;
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

Bno FS::zone2Block(Zno zoneNumber)
{
	return zoneNumber * g_BlocksPerZone;
}

ErrorCode FS::readInode(Ino inodeNumber, void* buffer)
{
	if (inodeNumber == 0 || inodeNumber > g_Superblock.s_ninodes)
	{
		return ERROR_INVALID_INODE_NUMBER;
	}
	inodeNumber--;
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

ErrorCode FS::readOneZoneData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	Bno blockNumber = zone2Block(zoneNumber);
	for (uint32_t i = 0; i < g_BlocksPerZone; i++)
	{
		if (offset >= g_BlockSize)
		{
			offset -= g_BlockSize;
			continue;
		}
		if (offset == 0 && sizeToRead >= g_BlockSize)
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
			uint32_t toRead = std::min(g_BlockSize - offset, sizeToRead);
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

ErrorCode FS::readSingleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	IndirectBlock indirectBlock;
	ErrorCode err = g_BlockDevice.readBlock(zone2Block(zoneNumber), &indirectBlock);
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

ErrorCode FS::readDoubleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	IndirectBlock indirectBlock;
	ErrorCode err = g_BlockDevice.readBlock(zone2Block(zoneNumber), &indirectBlock);
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

ErrorCode FS::readTripleIndirectData(Zno zoneNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	IndirectBlock indirectBlock;
	ErrorCode err = g_BlockDevice.readBlock(zone2Block(zoneNumber), &indirectBlock);
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

ErrorCode FS::readInodeData(Ino inodeNumber, uint8_t *buffer, uint32_t sizeToRead, uint32_t offset)
{
	MinixInode3 inode;
	ErrorCode err = readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}

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

ErrorCode FS::readInodeFullData(Ino inodeNumber, uint8_t *buffer)
{
	MinixInode3 inode;
	ErrorCode err = readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	return readInodeData(inodeNumber, buffer, inode.i_size, 0);
}

Ino FS::getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name, ErrorCode &outError)
{
	MinixInode3 parentInode;
	ErrorCode err = readInode(parentInodeNumber, &parentInode);
	if (err != SUCCESS)
	{
		outError = err;
		return 0;
	}
	if (!parentInode.isDirectory())
	{
		outError = ERROR_NOT_DIRECTORY;
		return 0;
	}
	uint32_t dirSize = parentInode.i_size;
	uint8_t *dirData = static_cast<uint8_t*>(malloc(dirSize));
	if (dirData == nullptr)
	{
		outError = ERROR_CANNOT_ALLOCATE_MEMORY;
		return 0;
	}
	err = readInodeFullData(parentInodeNumber, dirData);
	if (err != SUCCESS)
	{
		outError = err;
		free(dirData);
		return 0;
	}
	if (dirSize % sizeof(DirEntryOnDisk) != 0)
	{
		free(dirData);
		outError = ERROR_FS_BROKEN;
		return 0;
	}
	for (uint32_t offset = 0; offset < dirSize; offset += sizeof(DirEntryOnDisk))
	{
		DirEntryOnDisk *entry = reinterpret_cast<DirEntryOnDisk*>(dirData + offset);
		std::string entryName = char60ToString(entry->d_name);
		if (entryName == name && entry->d_inode != 0)
		{
			Ino inodeNumber = entry->d_inode;
			free(dirData);
			outError = SUCCESS;
			return inodeNumber;
		}
	}
	free(dirData);
	outError = ERROR_FILE_NOT_FOUND;
	return 0;
}

Ino FS::getInodeFromPath(const std::string &path, ErrorCode &outError)
{
	Ino currentInode = MINIX3_ROOT_INODE;
	std::vector<std::string> components = splitPath(path);
	for (const std::string &component: components)
	{
		ErrorCode err;
		currentInode = getInodeFromParentAndName(currentInode, component, err);
		if (err != SUCCESS)
		{
			outError = err;
			return 0;
		}
	}
	outError = SUCCESS;
	return currentInode;
}

uint16_t FS::getBlockSize() const
{
	return g_BlockSize;
}

struct stat FS::attrToStat(const Attribute &attr) const
{
	struct stat st;
	st.st_ino = attr.ino;
	st.st_mode = attr.mode;
	st.st_nlink = attr.nlinks;
	st.st_uid = attr.uid;
	st.st_gid = attr.gid;
	st.st_size = attr.size;
	st.st_atime = attr.atime;
	st.st_mtime = attr.mtime;
	st.st_ctime = attr.ctime;
	st.st_blocks = attr.blocks;
	st.st_rdev = attr.rdev;
	return st;
}

std::vector<DirEntry> FS::listDir(const std::string &path, uint32_t offset, uint32_t count, ErrorCode &outError)
{
	std::vector<DirEntry> entries;
	Ino dirInodeNumber = getInodeFromPath(path, outError);
	if (outError != SUCCESS)
	{
		return entries;
	}
	MinixInode3 dirInode;
	ErrorCode err = readInode(dirInodeNumber, &dirInode);
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
	err = readInodeData(dirInodeNumber, dirData, entriesToRead * sizeof(DirEntryOnDisk), offset * sizeof(DirEntryOnDisk));
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
			err = readInode(entryOnDisk->d_inode, &entryInode);
			if (err != SUCCESS) continue;
			entry.attribute = getAttributeFromInode(entryOnDisk->d_inode, err);
			if (err != SUCCESS) continue;
			entries.push_back(entry);
		}
	}
	free(dirData);
	outError = SUCCESS;
	return entries;
}

std::vector<DirEntry> FS::listDir(const std::string &path, ErrorCode &outError)
{
	std::vector<DirEntry> entries;
	Ino dirInodeNumber = getInodeFromPath(path, outError);
	if (outError != SUCCESS)
	{
		return entries;
	}
	MinixInode3 dirInode;
	ErrorCode err = readInode(dirInodeNumber, &dirInode);
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
	return listDir(path, 0, dirSize / sizeof(DirEntryOnDisk), outError);
}

uint32_t FS::readFile(const std::string &path, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError)
{
	Ino fileInodeNumber = getInodeFromPath(path, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	MinixInode3 fileInode;
	ErrorCode err = readInode(fileInodeNumber, &fileInode);
	if (err != SUCCESS)
	{
		outError = err;
		return 0;
	}
	if (!fileInode.isRegularFile())
	{
		outError = ERROR_NOT_REGULAR_FILE;
		return 0;
	}
	if (offset >= fileInode.i_size)
	{
		outError = ERROR_READ_FILE_END;
		return 0;
	}
	if (sizeToRead > fileInode.i_size - offset)
	{
		sizeToRead = fileInode.i_size - offset;
	}
	outError = readInodeData(fileInodeNumber, buffer, sizeToRead, offset);
	if (outError != SUCCESS)
	{
		return 0;
	}
	return sizeToRead;
}

Attribute FS::getAttributeFromInode(Ino inodeNumber, ErrorCode &outError)
{
	Attribute attr = {};
	MinixInode3 inode;
	ErrorCode err = readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		outError = err;
		return attr;
	}
	attr.ino = inodeNumber;
	attr.mode = inode.i_mode;
	attr.size = inode.isRegularFile() ? inode.i_size : 0;
	attr.nlinks = inode.i_nlinks;
	attr.uid = inode.i_uid;
	attr.gid = inode.i_gid;
	attr.atime = inode.i_atime;
	attr.mtime = inode.i_mtime;
	attr.ctime = inode.i_ctime;
	attr.blocks = (inode.i_size + g_BlockSize - 1) / g_BlockSize;
	attr.rdev = 0;
	outError = SUCCESS;
	return attr;
}

Attribute FS::getFileAttribute(const std::string &path, ErrorCode &outError)
{
	Attribute attr = {};
	Ino inodeNumber = getInodeFromPath(path, outError);
	if (outError != SUCCESS)
	{
		return attr;
	}
	return getAttributeFromInode(inodeNumber, outError);
}