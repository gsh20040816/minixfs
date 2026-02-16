#pragma once

#include "BlockDevice.h"
#include "Allocator.h"

struct TransactionManager
{
	BlockDevice *blockDevice;
	Allocator *imapAllocator;
	Allocator *zmapAllocator;
	bool isInTransaction = false;
	bool writeLocked = false;
	ErrorCode writeLockedReason = SUCCESS;
	bool isWriteLocked() const;
	void setBlockDevice(BlockDevice &blockDevice);
	void setImapAllocator(Allocator &imapAllocator);
	void setZmapAllocator(Allocator &zmapAllocator);
	ErrorCode beginTransaction();
	ErrorCode revertTransaction();
	ErrorCode commitTransaction();
	ErrorCode setWriteLock(ErrorCode reason);
};