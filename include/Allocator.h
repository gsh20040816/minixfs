#pragma once

#include <cstdint>
#include <unordered_map>
#include "BlockDevice.h"
#include "Layout.h"
#include "Errors.h"

struct Allocator
{
	BlockDevice *blockDevice = nullptr;
	uint32_t bmapStartBlock;
	uint32_t firstFreeBmap;
	uint32_t totalBmaps;
	uint32_t totalBlocks;
	uint32_t blockSize;
	uint32_t bitsPerBlock;
	bool isInTransaction = false;
	uint8_t *bmapCache = nullptr;
	uint8_t *isDirtyBlock = nullptr;
	std::unordered_map<uint32_t, uint8_t> transactionDirtyBlocks;
	bool setBit(uint32_t idx, bool value);

	Allocator();
	~Allocator();
	void setBlockDevice(BlockDevice &bd);
	ErrorCode init(uint32_t bmapStartBlock, uint32_t totalBmaps, uint32_t firstFreeBmap, uint32_t blockSize);
	ErrorCode sync();
	uint32_t allocateBmap(ErrorCode &outError);
	ErrorCode freeBmap(uint32_t idx);
	ErrorCode beginTransaction();
	ErrorCode revertTransaction();
	ErrorCode commitTransaction();
};
