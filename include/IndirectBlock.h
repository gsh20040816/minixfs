#pragma once

#include <cstdint>
#include "Constants.h"

struct IndirectBlock
{
	uint32_t zones[MINIX3_MAX_BLOCK_SIZE / sizeof(uint32_t)];
};