#include "FS.h"
#include "Constants.h"
#include "Utils.h"
#include "Inode.h"
#include "IndirectBlock.h"
#include "DirEntry.h"
#include <cstring>

FS::FS(): g_BlockDevice(), g_Superblock() {}

FS::FS(const std::string &devicePath): g_BlockDevice(devicePath), g_Superblock() {}

void FS::setDevicePath(const std::string &devicePath)
{
	g_BlockDevice.setDevicePath(devicePath);
}

ErrorCode FS::mount()
{
	BlockDevice &bd = g_BlockDevice;
	ErrorCode err = bd.open();
	if (err != SUCCESS)
	{
		return err;
	}

	err = bd.readBytes(MINIX3_SUPERBLOCK_OFFSET, &g_Superblock, sizeof(MinixSuperblock3));
	if (err != SUCCESS)
	{
		bd.close();
		return err;
	}

	MinixSuperblock3 &sb = g_Superblock;
	Layout &layout = g_Layout;
	err = layout.fromSuperblock(sb);
	if (err != SUCCESS)
	{
		bd.close();
		return err;
	}
	bd.setBlockSize(layout.blockSize);

	g_InodeReader.setBlockDevice(bd);
	g_InodeReader.setLayout(layout);

	g_FileReader.setBlockDevice(bd);
	g_FileReader.setLayout(layout);

	g_DirReader.setInodeReader(g_InodeReader);
	g_DirReader.setFileReader(g_FileReader);

	g_PathResolver.setInodeReader(g_InodeReader);
	g_PathResolver.setDirReader(g_DirReader);

	return SUCCESS;
}

ErrorCode FS::unmount()
{
	return g_BlockDevice.close();
}

uint16_t FS::getBlockSize() const
{
	return g_Layout.blockSize;
}

struct stat FS::attrToStat(const Attribute &attr) const
{
	struct stat st;
	st.st_ino = attr.ino;
	st.st_mode = attr.mode;
	st.st_nlink = attr.nlinks;
	st.st_uid = attr.uid;
	st.st_gid = attr.gid;
	st.st_size = attr.size;
	st.st_atime = attr.atime;
	st.st_mtime = attr.mtime;
	st.st_ctime = attr.ctime;
	st.st_blocks = attr.blocks;
	st.st_rdev = attr.rdev;
	return st;
}

uint32_t FS::readFile(const std::string &path, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError)
{
	Ino fileInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	MinixInode3 fileInode;
	ErrorCode err = g_InodeReader.readInode(fileInodeNumber, &fileInode);
	if (err != SUCCESS)
	{
		outError = err;
		return 0;
	}
	if (!fileInode.isRegularFile())
	{
		outError = ERROR_NOT_REGULAR_FILE;
		return 0;
	}
	if (offset >= fileInode.i_size)
	{
		outError = ERROR_READ_FILE_END;
		return 0;
	}
	if (sizeToRead > fileInode.i_size - offset)
	{
		sizeToRead = fileInode.i_size - offset;
	}
	outError = g_FileReader.readFile(fileInode, buffer, sizeToRead, offset);
	if (outError != SUCCESS)
	{
		return 0;
	}
	return sizeToRead;
}

Attribute FS::getFileAttribute(const std::string &path, ErrorCode &outError)
{
	Attribute attr = {};
	Ino inodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return attr;
	}
	return g_InodeReader.readAttribute(inodeNumber, outError);
}

uint32_t FS::getDirectorySize(const std::string &path, ErrorCode &outError)
{
	Ino dirInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	MinixInode3 dirInode;
	ErrorCode err = g_InodeReader.readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		outError = err;
		return 0;
	}
	if (!dirInode.isDirectory())
	{
		outError = ERROR_NOT_DIRECTORY;
		return 0;
	}
	outError = SUCCESS;
	return dirInode.i_size / sizeof(DirEntryOnDisk);
}

std::vector<DirEntry> FS::listDir(const std::string &path, uint32_t offset, uint32_t count, ErrorCode &outError)
{
	Ino dirInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return {};
	}
	return g_DirReader.readDir(dirInodeNumber, offset, count, outError);
}

std::vector<DirEntry> FS::listDir(const std::string &path, ErrorCode &outError)
{
	Ino dirInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return {};
	}
	MinixInode3 dirInode;
	ErrorCode err = g_InodeReader.readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		outError = err;
		return {};
	}
	if (!dirInode.isDirectory())
	{
		outError = ERROR_NOT_DIRECTORY;
		return {};
	}
	uint32_t totalEntries = dirInode.i_size / sizeof(DirEntryOnDisk);
	return g_DirReader.readDir(dirInodeNumber, 0, totalEntries, outError);
}