#include "TransactionManager.h"

void TransactionManager::setBlockDevice(BlockDevice &blockDevice)
{
	this->blockDevice = &blockDevice;
}

void TransactionManager::setImapAllocator(Allocator &imapAllocator)
{
	this->imapAllocator = &imapAllocator;
}

void TransactionManager::setZmapAllocator(Allocator &zmapAllocator)
{
	this->zmapAllocator = &zmapAllocator;
}

ErrorCode TransactionManager::beginTransaction()
{
	if (isInTransaction)
	{
		return ERROR_IS_IN_TRANSACTION;
	}
	ErrorCode err;
	err = blockDevice->beginTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	err = imapAllocator->beginTransaction();
	if (err != SUCCESS)
	{
		blockDevice->revertTransaction();
		return err;
	}
	err = zmapAllocator->beginTransaction();
	if (err != SUCCESS)
	{
		blockDevice->revertTransaction();
		imapAllocator->revertTransaction();
		return err;
	}
	isInTransaction = true;
	return SUCCESS;
}

ErrorCode TransactionManager::revertTransaction()
{
	if (!isInTransaction)
	{
		return ERROR_IS_NOT_IN_TRANSACTION;
	}
	ErrorCode err;
	err = blockDevice->revertTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	err = imapAllocator->revertTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	err = zmapAllocator->revertTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	isInTransaction = false;
	return SUCCESS;
}

ErrorCode TransactionManager::commitTransaction()
{
	if (!isInTransaction)
	{
		return ERROR_IS_NOT_IN_TRANSACTION;
	}
	ErrorCode err;
	err = blockDevice->commitTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	err = imapAllocator->commitTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	err = zmapAllocator->commitTransaction();
	if (err != SUCCESS)
	{
		return err;
	}
	isInTransaction = false;
	return SUCCESS;
}