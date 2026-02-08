#include "Allocator.h"

Allocator::Allocator() {}

Allocator::~Allocator()
{
	if (blockDevice != nullptr && bmapCache != nullptr && isDirtyBlock != nullptr)
	{
		sync();
	}
	if (bmapCache != nullptr)
	{
		free(bmapCache);
		bmapCache = nullptr;
	}
	if (isDirtyBlock != nullptr)
	{
		free(isDirtyBlock);
		isDirtyBlock = nullptr;
	}
}

void Allocator::setBlockDevice(BlockDevice &bd)
{
	this->blockDevice = &bd;
}

ErrorCode Allocator::init(uint32_t bmapStartBlock, uint32_t totalBmaps, uint32_t firstFreeBmap, uint32_t blockSize)
{
	this->bmapStartBlock = bmapStartBlock;
	this->totalBmaps = totalBmaps;
	this->firstFreeBmap = firstFreeBmap;
	this->blockSize = blockSize;
	this->bitsPerBlock = blockSize * sizeof(uint8_t) * 8;
	this->totalBlocks = (totalBmaps + this->bitsPerBlock - 1) / this->bitsPerBlock;
	this->bmapCache = static_cast<uint8_t*>(malloc(totalBlocks * blockSize));
	if (bmapCache == nullptr)
	{
		return ERROR_CANNOT_ALLOCATE_MEMORY;
	}
	ErrorCode err = blockDevice->readBytes(bmapStartBlock * blockSize, bmapCache, totalBlocks * blockSize);
	if (err != SUCCESS)
	{
		free(bmapCache);
		bmapCache = nullptr;
		return err;
	}
	this->isDirtyBlock = static_cast<uint8_t*>(calloc(totalBlocks, sizeof(uint8_t)));
	if (isDirtyBlock == nullptr)
	{
		free(bmapCache);
		bmapCache = nullptr;
		return ERROR_CANNOT_ALLOCATE_MEMORY;
	}
	return SUCCESS;
}

ErrorCode Allocator::sync()
{
	for (uint32_t i = 0; i < totalBlocks; i++)
	{
		if (isDirtyBlock[i])
		{
			ErrorCode err = blockDevice->writeBlock(bmapStartBlock + i, bmapCache + i * blockSize);
			if (err != SUCCESS)
			{
				return err;
			}
			isDirtyBlock[i] = 0;
		}
	}
	return SUCCESS;
}

bool Allocator::setBit(uint32_t idx, bool value)
{
	if (idx >= totalBmaps || idx < firstFreeBmap)
	{
		return false;
	}
	uint32_t block = idx / bitsPerBlock;
	uint32_t bitInBlock = idx % bitsPerBlock;
	uint32_t byteInBlock = bitInBlock / 8;
	uint8_t bitMask = 1 << (bitInBlock % 8);
	uint8_t &byte = bmapCache[block * blockSize + byteInBlock];
	bool oldValue = (byte & bitMask) != 0;
	if (value == oldValue)
	{
		return false;
	}
	if (value)
	{
		byte |= bitMask;
	}
	else
	{
		byte &= ~bitMask;
	}
	isDirtyBlock[block] = 1;
	return true;
}

uint32_t Allocator::allocateBmap(ErrorCode &outError)
{
	for(uint32_t i = firstFreeBmap; i < totalBmaps; i++)
	{
		if (setBit(i, true))
		{
			outError = SUCCESS;
			return i;
		}
	}
	outError = ERROR_CANNOT_ALLOCATE_BMAP;
	return 0;
}

ErrorCode Allocator::freeBmap(uint32_t idx)
{
	if (idx >= totalBmaps || idx < firstFreeBmap)
	{
		return ERROR_INVALID_BMAP_INDEX;
	}
	if (!setBit(idx, false))
	{
		return ERROR_FREEING_UNALLOCATED_BMAP;
	}
	return SUCCESS;
}