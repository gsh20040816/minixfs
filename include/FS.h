#pragma once

#include "BlockDevice.h"
#include "Superblock.h"
#include "Type.h"
#include "Errors.h"
#include "Layout.h"
#include "DirEntry.h"
#include "InodeReader.h"
#include "FileReader.h"
#include "DirReader.h"
#include "PathResolver.h"
#include <string>
#include <cstdint>
#include <vector>
#include <sys/stat.h>

class FS
{
private:
	BlockDevice g_BlockDevice;
	MinixSuperblock3 g_Superblock;
	Layout g_Layout;
	InodeReader g_InodeReader;
	FileReader g_FileReader;
	DirReader g_DirReader;
	PathResolver g_PathResolver;

	Ino getInodeFromParentAndName(Ino parentInodeNumber, const std::string &name, ErrorCode &outError);
public:
	FS();
	FS(const std::string &devicePath);
	void setDevicePath(const std::string &devicePath);
	ErrorCode mount();
	ErrorCode unmount();
	uint16_t getBlockSize() const;
	uint32_t getDirectorySize(const std::string &path, ErrorCode &outError);
	std::vector<DirEntry> listDir(const std::string &path, uint32_t offset, uint32_t count, ErrorCode &outError);
	std::vector<DirEntry> listDir(const std::string &path, ErrorCode &outError);
	uint32_t readFile(const std::string &path, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError);
	struct stat getFileStat(const std::string &path, ErrorCode &outError);
};