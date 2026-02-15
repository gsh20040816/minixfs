#pragma once

#include "BlockDevice.h"
#include "Superblock.h"
#include "Type.h"
#include "Errors.h"
#include "Layout.h"
#include "DirEntry.h"
#include "InodeReader.h"
#include "InodeWriter.h"
#include "FileReader.h"
#include "FileWriter.h"
#include "FileCreator.h"
#include "FileMapper.h"
#include "FileCounter.h"
#include "FileDeleter.h"
#include "FileRenamer.h"
#include "DirReader.h"
#include "DirWriter.h"
#include "DirCreator.h"
#include "DirDeleter.h"
#include "PathResolver.h"
#include "LinkReader.h"
#include "Allocator.h"
#include <string>
#include <cstdint>
#include <vector>
#include <sys/stat.h>
#include <sys/statvfs.h>

class FS
{
private:
	BlockDevice g_BlockDevice;
	MinixSuperblock3 g_Superblock;
	Layout g_Layout;
	InodeReader g_InodeReader;
	InodeWriter g_InodeWriter;
	FileMapper g_FileMapper;
	FileReader g_FileReader;
	FileWriter g_FileWriter;
	FileCreator g_FileCreator;
	DirReader g_DirReader;
	DirWriter g_DirWriter;
	LinkReader g_LinkReader;
	PathResolver g_PathResolver;
	Allocator g_imapAllocator;
	Allocator g_zmapAllocator;
	FileCounter g_FileCounter;
	FileDeleter g_FileDeleter;
	FileLinker g_FileLinker;
	FileRenamer g_FileRenamer;
	DirCreator g_DirCreator;
	DirDeleter g_DirDeleter;
public:
	FS();
	FS(const std::string &devicePath);
	void setDevicePath(const std::string &devicePath);
	ErrorCode mount();
	ErrorCode unmount();
	uint16_t getBlockSize() const;
	uint32_t getDirectorySize(Ino inodeNumber, ErrorCode &outError);
	uint32_t getDirectorySize(const std::string &path, ErrorCode &outError);
	std::vector<DirEntry> listDir(Ino inodeNumber, uint32_t offset, uint32_t count, ErrorCode &outError);
	std::vector<DirEntry> listDir(Ino inodeNumber, ErrorCode &outError);
	std::vector<DirEntry> listDir(const std::string &path, uint32_t offset, uint32_t count, ErrorCode &outError);
	std::vector<DirEntry> listDir(const std::string &path, ErrorCode &outError);
	uint32_t readFile(const std::string &path, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError);
	uint32_t writeFile(const std::string &path, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite, ErrorCode &outError);
	uint32_t readFile(Ino inodeNumber, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError);
	uint32_t writeFile(Ino inodeNumber, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite, ErrorCode &outError);
	struct stat getFileStat(const std::string &path, ErrorCode &outError);
	std::string readLink(const std::string &path, ErrorCode &outError);
	struct statvfs getFSStat(ErrorCode &outError);
	ErrorCode openFile(const std::string &path, Ino &outInodeNumber, uint32_t flags);
	ErrorCode closeFile(Ino inodeNumber);
	ErrorCode unlinkFile(const std::string &path);
	Ino createFile(const std::string &path, const std::string &name, uint16_t mode, uint16_t uid, uint16_t gid, ErrorCode &outError);
	ErrorCode truncateFile(const std::string &path, uint32_t newSize);
	ErrorCode truncateFile(Ino inodeNumber, uint32_t newSize);
	ErrorCode renameFile(const std::string &from, const std::string &to, bool failIfDstExists);
	ErrorCode mkdir(const std::string &path, uint16_t mode, uint16_t uid, uint16_t gid);
};