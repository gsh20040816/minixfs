#pragma once

#include <cstdint>
#include "BlockDevice.h"
#include "Layout.h"
#include "Errors.h"

struct Allocator
{
	BlockDevice *blockDevice = nullptr;
	uint32_t bmapStartBlock;
	uint32_t totalBmaps;
	uint32_t totalBlocks;
	uint32_t blockSize;
	uint32_t bitsPerBlock;
	uint8_t *bmapCache = nullptr;
	uint8_t *isDirtyBlock = nullptr;
	bool setBit(uint32_t idx, bool value);

	Allocator();
	~Allocator();
	void setBlockDevice(BlockDevice &bd);
	ErrorCode init(uint32_t bmapStartBlock, uint32_t totalBmaps, uint32_t blockSize);
	ErrorCode sync();
	uint32_t allocateBmap(ErrorCode &outError);
	ErrorCode freeBmap(uint32_t idx);
};