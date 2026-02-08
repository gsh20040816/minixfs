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
	BlockDevice *blockDevice;
	InodeReader *inodeReader;
	Allocator *zmapAllocator;
	void setBlockDevice(BlockDevice &blockDevice);
	void setInodeReader(InodeReader &inodeReader);
	void setZonesPerIndirectBlock(uint32_t zonesPerIndirectBlock);
	void setBlocksPerZone(uint32_t blocksPerZone);
	void setZmapAllocator(Allocator &zmapAllocator);
	ErrorCode mapLogicalToPhysical(MinixInode3 &inode, Zno logicalZoneIndex, Zno &outPhysicalZoneIndex, bool allocateIfNotMapped = false);
};