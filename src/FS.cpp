#include "FS.h"
#include "Constants.h"
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

	err = bd.readBlock(MINIX3_SUPERBLOCK_OFFSET / MINIX3_DEFAULT_BLOCK_SIZE, &g_Superblock, MINIX3_DEFAULT_BLOCK_SIZE);
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
	ErrorCode err = g_BlockDevice.readBlock(inodeBlockNumber, blockBuffer, g_BlockSize);
	if (err != SUCCESS)
	{
		free(blockBuffer);
		return err;
	}
	memcpy(buffer, static_cast<uint8_t*>(blockBuffer) + inodeOffset, MINIX3_INODE_SIZE);
	free(blockBuffer);
	return SUCCESS;
}