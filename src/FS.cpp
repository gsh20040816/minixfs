#include "FS.h"
#include "Constants.h"
#include "Utils.h"
#include "Inode.h"
#include "IndirectBlock.h"
#include "DirEntry.h"
#include <cstring>
#include <fcntl.h>

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
	bd.setZoneSize(layout.zoneSize);

	g_InodeReader.setBlockDevice(bd);
	g_InodeReader.setLayout(layout);

	g_InodeWriter.setBlockDevice(bd);
	g_InodeWriter.setLayout(layout);

	g_FileMapper.setBlockDevice(bd);
	g_FileMapper.setInodeReader(g_InodeReader);
	g_FileMapper.setZonesPerIndirectBlock(layout.zonesPerIndirectBlock);
	g_FileMapper.setBlocksPerZone(layout.blocksPerZone);
	g_FileMapper.setBlockSize(layout.blockSize);

	g_FileReader.setBlockDevice(bd);
	g_FileReader.setLayout(layout);
	g_FileReader.setFileMapper(g_FileMapper);

	g_FileWriter.setBlockDevice(bd);
	g_FileWriter.setLayout(layout);
	g_FileWriter.setFileMapper(g_FileMapper);
	g_FileWriter.setInodeReader(g_InodeReader);
	g_FileWriter.setInodeWriter(g_InodeWriter);

	g_DirReader.setInodeReader(g_InodeReader);
	g_DirReader.setFileReader(g_FileReader);

	g_DirWriter.setInodeReader(g_InodeReader);
	g_DirWriter.setInodeWriter(g_InodeWriter);
	g_DirWriter.setDirReader(g_DirReader);
	g_DirWriter.setFileWriter(g_FileWriter);

	g_FileCreator.setInodeReader(g_InodeReader);
	g_FileCreator.setInodeWriter(g_InodeWriter);
	g_FileCreator.setDirReader(g_DirReader);
	g_FileCreator.setDirWriter(g_DirWriter);
	g_FileCreator.setImapAllocator(g_imapAllocator);

	g_LinkReader.setInodeReader(g_InodeReader);
	g_LinkReader.setFileReader(g_FileReader);

	g_PathResolver.setInodeReader(g_InodeReader);
	g_PathResolver.setDirReader(g_DirReader);
	g_PathResolver.setLinkReader(g_LinkReader);

	g_FileDeleter.setImapAllocator(g_imapAllocator);
	g_FileDeleter.setFileWriter(g_FileWriter);
	g_FileDeleter.setFileCounter(g_FileCounter);
	g_FileDeleter.setDirReader(g_DirReader);
	g_FileDeleter.setDirWriter(g_DirWriter);
	g_FileDeleter.setInodeReader(g_InodeReader);
	g_FileDeleter.setInodeWriter(g_InodeWriter);

	g_FileLinker.setInodeReader(g_InodeReader);
	g_FileLinker.setInodeWriter(g_InodeWriter);
	g_FileLinker.setDirWriter(g_DirWriter);
	g_FileLinker.setPathResolver(g_PathResolver);

	g_DirDeleter.setInodeReader(g_InodeReader);
	g_DirDeleter.setInodeWriter(g_InodeWriter);
	g_DirDeleter.setDirReader(g_DirReader);
	g_DirDeleter.setFileDeleter(g_FileDeleter);
	g_DirDeleter.setPathResolver(g_PathResolver);

	g_FileRenamer.setDirReader(g_DirReader);
	g_FileRenamer.setDirWriter(g_DirWriter);
	g_FileRenamer.setInodeReader(g_InodeReader);
	g_FileRenamer.setInodeWriter(g_InodeWriter);
	g_FileRenamer.setFileDeleter(g_FileDeleter);
	g_FileRenamer.setPathResolver(g_PathResolver);
	g_FileRenamer.setFileLinker(g_FileLinker);
	g_FileRenamer.setDirDeleter(g_DirDeleter);

	g_DirCreator.setInodeReader(g_InodeReader);
	g_DirCreator.setInodeWriter(g_InodeWriter);
	g_DirCreator.setPathResolver(g_PathResolver);
	g_DirCreator.setFileCreator(g_FileCreator);
	g_DirCreator.setFileLinker(g_FileLinker);

	g_AttributeUpdater.setInodeReader(g_InodeReader);
	g_AttributeUpdater.setInodeWriter(g_InodeWriter);

	g_imapAllocator.setBlockDevice(bd);
	err = g_imapAllocator.init(layout.imapStart, layout.totalInodes + 1, 1, layout.blockSize);
	if (err != SUCCESS)
	{
		bd.close();
		return err;
	}

	g_zmapAllocator.setBlockDevice(bd);
	err = g_zmapAllocator.init(layout.zmapStart, layout.totalZones, layout.firstDataZone, layout.blockSize);
	if (err != SUCCESS)
	{
		bd.close();
		return err;
	}
	g_FileMapper.setZmapAllocator(g_zmapAllocator);

	return SUCCESS;
}

ErrorCode FS::unmount()
{
	ErrorCode err = SUCCESS;
	ErrorCode imapErr = g_imapAllocator.sync();
	ErrorCode zmapErr = g_zmapAllocator.sync();
	if (imapErr != SUCCESS)
	{
		return imapErr;
	}
	if (zmapErr != SUCCESS)
	{
		return zmapErr;
	}
	return g_BlockDevice.close();
}

uint16_t FS::getBlockSize() const
{
	return g_Layout.blockSize;
}

uint32_t FS::readFile(Ino inodeNumber, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError)
{
	MinixInode3 fileInode;
	ErrorCode err = g_InodeReader.readInode(inodeNumber, &fileInode);
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
		outError = SUCCESS;
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

uint32_t FS::writeFile(Ino inodeNumber, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite, ErrorCode &outError)
{
	ErrorCode err = g_FileWriter.writeFile(inodeNumber, data, offset, sizeToWrite);
	if (err != SUCCESS)
	{
		outError = err;
		return 0;
	}
	outError = SUCCESS;
	return sizeToWrite;
}

uint32_t FS::readFile(const std::string &path, uint8_t *buffer, uint32_t offset, uint32_t sizeToRead, ErrorCode &outError)
{
	Ino inodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	return readFile(inodeNumber, buffer, offset, sizeToRead, outError);
}

uint32_t FS::writeFile(const std::string &path, const uint8_t *data, uint32_t offset, uint32_t sizeToWrite, ErrorCode &outError)
{
	Ino inodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	return writeFile(inodeNumber, data, offset, sizeToWrite, outError);
}

Ino FS::createFile(const std::string &path, const std::string &name, uint16_t mode, uint16_t uid, uint16_t gid, ErrorCode &outError)
{
	Ino parentInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	Ino newInodeNumber = g_FileCreator.createFile(parentInodeNumber, name, mode, uid, gid, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	return newInodeNumber;
}

ErrorCode FS::truncateFile(const std::string &path, uint32_t newSize)
{
	ErrorCode err;
	Ino inodeNumber = g_PathResolver.resolvePath(path, err);
	if (err != SUCCESS)
	{
		return err;
	}
	return truncateFile(inodeNumber, newSize);
}

ErrorCode FS::truncateFile(Ino inodeNumber, uint32_t newSize)
{
	MinixInode3 inode;
	ErrorCode err = g_InodeReader.readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!inode.isRegularFile())
	{
		return ERROR_NOT_REGULAR_FILE;
	}
	return g_FileWriter.truncateFile(inodeNumber, newSize);
}

ErrorCode FS::renameFile(const std::string &from, const std::string &to, bool failIfDstExists)
{
	auto [srcParentPath, srcName] = splitPathIntoDirAndBase(from);
	auto [dstParentPath, dstName] = splitPathIntoDirAndBase(to);
	ErrorCode err;
	Ino srcParentInodeNumber = g_PathResolver.resolvePath(srcParentPath, err);
	if (err != SUCCESS)
	{
		return err;
	}
	Ino dstParentInodeNumber = g_PathResolver.resolvePath(dstParentPath, err);
	if (err != SUCCESS)
	{
		return err;
	}
	return g_FileRenamer.rename(srcParentInodeNumber, srcName, dstParentInodeNumber, dstName, failIfDstExists);
}

ErrorCode FS::openFile(const std::string &path, Ino &outInodeNumber, uint32_t flags)
{
	ErrorCode err;
	outInodeNumber = g_PathResolver.resolvePath(path, err);
	if (err != SUCCESS)
	{
		return err;
	}
	MinixInode3 inode;
	err = g_InodeReader.readInode(outInodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (!inode.isRegularFile())
	{
		return ERROR_NOT_REGULAR_FILE;
	}
	if (flags & O_TRUNC)
	{
		err = g_FileWriter.truncateFile(outInodeNumber, 0);
		if (err != SUCCESS)
		{
			return err;
		}
	}
	g_FileCounter.add(outInodeNumber);
	return SUCCESS;
}

ErrorCode FS::closeFile(Ino inodeNumber)
{
	g_FileCounter.remove(inodeNumber);
	MinixInode3 inode;
	ErrorCode err = g_InodeReader.readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (inode.i_nlinks == 0 && g_FileCounter.empty(inodeNumber))
	{
		return g_FileDeleter.deleteFile(inodeNumber);
	}
	return SUCCESS;
}

ErrorCode FS::linkFile(const std::string &existingPath, const std::string &newPath)
{
	ErrorCode err;
	Ino srcInodeNumber = g_PathResolver.resolvePath(existingPath, err, MINIX3_ROOT_INODE, false);
	if (err != SUCCESS)
	{
		return err;
	}
	auto [dstParentPath, dstName] = splitPathIntoDirAndBase(newPath);
	Ino dstParentInodeNumber = g_PathResolver.resolvePath(dstParentPath, err);
	if (err != SUCCESS)
	{
		return err;
	}
	MinixInode3 srcInode;
	err = g_InodeReader.readInode(srcInodeNumber, &srcInode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (srcInode.isDirectory())
	{
		return ERROR_LINK_DIRECTORY;
	}
	return g_FileLinker.linkFile(dstParentInodeNumber, dstName, srcInodeNumber);
}

ErrorCode FS::unlinkFile(const std::string &path)
{
	ErrorCode err;
	auto [parentPath, name] = splitPathIntoDirAndBase(path);
	Ino parentInodeNumber = g_PathResolver.resolvePath(parentPath, err);
	if (err != SUCCESS)
	{
		return err;
	}
	uint32_t idx = g_PathResolver.getIdxFromParentAndName(parentInodeNumber, name, err);
	if (err != SUCCESS)
	{
		return err;
	}
	Ino inodeNumber = g_PathResolver.resolvePath(path, err, MINIX3_ROOT_INODE, false);
	if (err != SUCCESS)
	{
		return err;
	}
	MinixInode3 inode;
	err = g_InodeReader.readInode(inodeNumber, &inode);
	if (err != SUCCESS)
	{
		return err;
	}
	if (inode.isDirectory())
	{
		return ERROR_UNLINK_DIRECTORY;
	}
	return g_FileDeleter.unlinkFile(parentInodeNumber, idx);
}

ErrorCode FS::mkdir(const std::string &path, uint16_t mode, uint16_t uid, uint16_t gid)
{
	auto [parentPath, name] = splitPathIntoDirAndBase(path);
	ErrorCode err;
	Ino parentInodeNumber = g_PathResolver.resolvePath(parentPath, err);
	if (err != SUCCESS)
	{
		return err;
	}
	return g_DirCreator.createDir(parentInodeNumber, name, mode, uid, gid);
}

ErrorCode FS::rmdir(const std::string &path)
{
	if (path == "/")
	{
		return ERROR_DELETE_ROOT_DIR;
	}
	auto [parentPath, name] = splitPathIntoDirAndBase(path);
	ErrorCode err;
	Ino parentInodeNumber = g_PathResolver.resolvePath(parentPath, err);
	if (err != SUCCESS)
	{
		return err;
	}
	return g_DirDeleter.deleteDir(parentInodeNumber, name);
}

struct stat FS::getFileStat(const std::string &path, ErrorCode &outError)
{
	Ino inodeNumber = g_PathResolver.resolvePath(path, outError, MINIX3_ROOT_INODE, false);
	if (outError != SUCCESS)
	{
		return {};
	}
	return g_InodeReader.readStat(inodeNumber, outError);
}

uint32_t FS::getDirectorySize(Ino inodeNumber, ErrorCode &outError)
{
	MinixInode3 dirInode;
	ErrorCode err = g_InodeReader.readInode(inodeNumber, &dirInode);
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

uint32_t FS::getDirectorySize(const std::string &path, ErrorCode &outError)
{
	Ino dirInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	return getDirectorySize(dirInodeNumber, outError);
}

std::vector<DirEntry> FS::listDir(Ino inodeNumber, uint32_t offset, uint32_t count, ErrorCode &outError)
{
	return g_DirReader.readDir(inodeNumber, offset, count, outError);
}

std::vector<DirEntry> FS::listDir(const std::string &path, uint32_t offset, uint32_t count, ErrorCode &outError)
{
	Ino dirInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return {};
	}
	return listDir(dirInodeNumber, offset, count, outError);
}

std::vector<DirEntry> FS::listDir(Ino inodeNumber, ErrorCode &outError)
{
	MinixInode3 dirInode;
	ErrorCode err = g_InodeReader.readInode(inodeNumber, &dirInode);
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
	return g_DirReader.readDir(inodeNumber, 0, totalEntries, outError);
}

std::vector<DirEntry> FS::listDir(const std::string &path, ErrorCode &outError)
{
	Ino dirInodeNumber = g_PathResolver.resolvePath(path, outError);
	if (outError != SUCCESS)
	{
		return {};
	}
	return listDir(dirInodeNumber, outError);
}

std::string FS::readLink(const std::string &path, ErrorCode &outError)
{
	Ino inodeNumber = g_PathResolver.resolvePath(path, outError, MINIX3_ROOT_INODE, false);
	if (outError != SUCCESS)
	{
		return {};
	}
	std::string linkTarget;
	ErrorCode err = g_LinkReader.readLink(inodeNumber, linkTarget);
	if (err != SUCCESS)
	{
		outError = err;
		return {};
	}
	outError = SUCCESS;
	return linkTarget;
}

struct statvfs FS::getFSStat(ErrorCode &outError)
{
	struct statvfs st{};
	st.f_bsize = g_Layout.blockSize;
	st.f_frsize = g_Layout.blockSize;
	st.f_blocks = (g_Layout.totalZones - g_Layout.firstDataZone) * g_Layout.blocksPerZone;
	st.f_bfree = (g_Layout.totalZones - g_Layout.firstDataZone - g_zmapAllocator.getAllocatedCount()) * g_Layout.blocksPerZone;
	st.f_bavail = st.f_bfree;
	st.f_files = g_Layout.totalInodes;
	st.f_ffree = g_Layout.totalInodes - g_imapAllocator.getAllocatedCount();
	st.f_favail = st.f_ffree;
	st.f_fsid = 0;
	st.f_flag = 0;
	st.f_namemax = MINIX3_DIR_NAME_MAX;
	outError = SUCCESS;
	return st;
}

ErrorCode FS::chmod(const std::string &path, uint16_t mode)
{
	ErrorCode err;
	Ino inodeNumber = g_PathResolver.resolvePath(path, err, MINIX3_ROOT_INODE, false);
	if (err != SUCCESS)
	{
		return err;
	}
	return g_AttributeUpdater.chmod(inodeNumber, mode);
}

ErrorCode FS::chown(const std::string &path, uint16_t uid, uint16_t gid)
{
	ErrorCode err;
	Ino inodeNumber = g_PathResolver.resolvePath(path, err, MINIX3_ROOT_INODE, false);
	if (err != SUCCESS)
	{
		return err;
	}
	return g_AttributeUpdater.chown(inodeNumber, uid, gid);
}

ErrorCode FS::utimens(const std::string &path, uint32_t atime, uint32_t mtime)
{
	ErrorCode err;
	Ino inodeNumber = g_PathResolver.resolvePath(path, err, MINIX3_ROOT_INODE, false);
	if (err != SUCCESS)
	{
		return err;
	}
	return g_AttributeUpdater.utimens(inodeNumber, atime, mtime);
}