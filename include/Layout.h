#pragma once

#include <cstdint>
#include "Type.h"
#include "Errors.h"
#include "Superblock.h"

struct InodeOffset
{
	Bno blockNumber;
	uint32_t offsetInBlock;
};

struct Layout
{
	uint16_t blockSize;
	uint32_t zoneSize;
	Bno imapStart;
	Bno zmapStart;
	Bno inodeStart;
	Bno dataStart;
	uint32_t inodesPerBlock;
	uint32_t blocksPerZone;
	uint32_t indirectZonesPerBlock;
	uint32_t totalInodes;
	uint32_t totalZones;

	ErrorCode fromSuperblock(const MinixSuperblock3 &sb);
	Bno zone2Block(Zno zoneNumber);
	InodeOffset inodeOffset(Ino inodeNumber, ErrorCode &err);
};