#pragma once
#include <cstdint>

struct MinixSuperblock3
{
	uint32_t s_ninodes;
	uint16_t s_padding0;
	uint16_t s_imap_blocks;
	uint16_t s_zmap_blocks;
	uint16_t s_firstdatazone;
	uint16_t s_log_zone_size;
	uint16_t s_padding1;
	uint32_t s_max_size;
	uint32_t s_zones;
	uint16_t s_magic;
	uint16_t s_padding2;
	uint16_t s_blocksize;
	uint8_t  s_disk_version;
}__attribute__((packed));