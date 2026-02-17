#pragma once

#include <cstdint>
#include "Inode.h"
#include "InodeReader.h"
#include "IndirectBlock.h"
#include "BlockDevice.h"
#include "Allocator.h"

struct FileMapper
{
	uint32_t zonesPerIndirectBlock;
	uint32_t blocksPerZone;
	uint32_t blockSize;
	BlockDevice *blockDevice;
	InodeReader *inodeReader;
	Allocator *zmapAllocator;
	void setBlockDevice(BlockDevice &blockDevice);
	void setInodeReader(InodeReader &inodeReader);
	void setZonesPerIndirectBlock(uint32_t zonesPerIndirectBlock);
	void setBlocksPerZone(uint32_t blocksPerZone);
	void setBlockSize(uint32_t blockSize);
	void setZmapAllocator(Allocator &zmapAllocator);
	ErrorCode mapLogicalToPhysical(MinixInode3 &inode, Zno logicalZoneIndex, Zno &outPhysicalZoneIndex, bool allocateIfNotMapped = false, bool freeIfMapped = false, bool allocateWriteZero = true);
	ErrorCode freeLogicalZone(MinixInode3 &inode, Zno logicalZoneIndex);
	bool isIndirectBlockEmpty(const IndirectBlock &block) const;
};