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
	if (firstFreeBmap >= totalBmaps)
	{
		return ERROR_FS_BROKEN;
	}
	this->bmapStartBlock = bmapStartBlock;
	this->totalBmaps = totalBmaps;
	this->firstFreeBmap = firstFreeBmap;
	this->lstAllocated = firstFreeBmap;
	this->blockSize = blockSize;
	this->bitsPerBlock = blockSize * sizeof(uint8_t) * 8;
	this->totalBlocks = (totalBmaps - firstFreeBmap + 1 + this->bitsPerBlock - 1) / this->bitsPerBlock;
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
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
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
	uint32_t bitIdx = idx - firstFreeBmap + 1;
	uint32_t block = bitIdx / bitsPerBlock;
	uint32_t bitInBlock = bitIdx % bitsPerBlock;
	uint32_t byteInBlock = bitInBlock / 8;
	uint8_t bitMask = 1 << (bitInBlock % 8);
	if (isInTransaction)
	{
		auto it = transactionDirtyBlocks.find(block * blockSize + byteInBlock);
		if (it == transactionDirtyBlocks.end())
		{
			uint32_t byteIdx = block * blockSize + byteInBlock;
			uint8_t byte = bmapCache[byteIdx];
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
			transactionDirtyBlocks[byteIdx] = byte;
			return true;
		}
		else
		{
			uint8_t &byte = transactionDirtyBlocks[block * blockSize + byteInBlock];
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
			return true;
		}
	}
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
	for(uint32_t i = lstAllocated; true; )
	{
		if (setBit(i, true))
		{
			outError = SUCCESS;
			lstAllocated = i;
			return i;
		}
		i++;
		if (i >= totalBmaps)
		{
			i = firstFreeBmap;
		}
		if (i == lstAllocated)
		{
			break;
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

ErrorCode Allocator::beginTransaction()
{
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
	isInTransaction = true;
	return SUCCESS;
}

ErrorCode Allocator::revertTransaction()
{
	if (!isInTransaction)
	{
		return ERROR_FS_BROKEN;
	}
	isInTransaction = false;
	transactionDirtyBlocks.clear();
	return SUCCESS;
}

ErrorCode Allocator::commitTransaction()
{
	if (!isInTransaction)
	{
		return ERROR_FS_BROKEN;
	}
	for (const auto& [idx, state] : transactionDirtyBlocks)
	{
		bmapCache[idx] = state;
		isDirtyBlock[idx / blockSize] = 1;
	}
	transactionDirtyBlocks.clear();
	isInTransaction = false;
	return SUCCESS;
}

uint32_t Allocator::getAllocatedCount() const
{
	uint32_t count = 0;
	for(uint32_t i = firstFreeBmap; i < totalBmaps; i++)
	{
		uint32_t bitIdx = i - firstFreeBmap + 1;
		uint32_t block = bitIdx / bitsPerBlock;
		uint32_t bitInBlock = bitIdx % bitsPerBlock;
		uint32_t byteInBlock = bitInBlock / 8;
		uint8_t bitMask = 1 << (bitInBlock % 8);
		if ((bmapCache[block * blockSize + byteInBlock] & bitMask) != 0)
		{
			count++;
		}
	}
	return count;
}