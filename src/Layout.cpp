#include "Layout.h"
#include "Constants.h"

ErrorCode Layout::fromSuperblock(const MinixSuperblock3 &sb)
{
	if (sb.s_magic != MINIX3_MAGIC)
	{
		return ERROR_INVALID_SUPERBLOCK;
	}

	blockSize = sb.s_blocksize;
	if (blockSize != 1024 && blockSize != 2048 && blockSize != 4096)
	{
		return ERROR_FS_BROKEN;
	}
	if (sb.s_log_zone_size > 7)
	{
		return ERROR_FS_BROKEN;
	}

	zoneSize = blockSize << sb.s_log_zone_size;
	blocksPerZone = zoneSize / blockSize;
	zonesPerIndirectBlock = blockSize / sizeof(uint32_t);
	imapStart = MINIX3_IZONE_START_BLOCK;
	zmapStart = imapStart + sb.s_imap_blocks;
	inodeStart = zmapStart + sb.s_zmap_blocks;
	dataStart = sb.s_firstdatazone * blocksPerZone;
	inodesPerBlock = blockSize / MINIX3_INODE_SIZE;
	totalInodes = sb.s_ninodes;
	totalZones = sb.s_zones;
	firstDataZone = sb.s_firstdatazone;

	if (sb.s_imap_blocks * blockSize * 8 < totalInodes)
	{
		return ERROR_FS_BROKEN;
	}
	if (sb.s_zmap_blocks * blockSize * 8 < totalZones)
	{
		return ERROR_FS_BROKEN;
	}

	return SUCCESS;
}

Bno Layout::zone2Block(Zno zoneNumber)
{
	return zoneNumber * blocksPerZone;
}

InodeOffset Layout::inodeOffset(Ino inodeNumber, ErrorCode &err)
{
	if (inodeNumber == 0 || inodeNumber > totalInodes)
	{
		err = ERROR_INVALID_INODE_NUMBER;
		return {0, 0};
	}
	inodeNumber--;
	Bno blockNumber = inodeStart + inodeNumber / inodesPerBlock;
	uint32_t offsetInBlock = (inodeNumber % inodesPerBlock) * MINIX3_INODE_SIZE;
	err = SUCCESS;
	return {blockNumber, offsetInBlock};
}