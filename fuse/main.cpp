#define FUSE_USE_VERSION 35
#include "FS.h"
#include "Utils.h"
#include "Inode.h"
#include "Logger.h"
#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <cstring>

FS g_FileSystem;

static void *fs_init(fuse_conn_info *conn, fuse_config *cfg)
{
	cfg->kernel_cache = 1;
	cfg->use_ino = 1;
	Logger::log("Filesystem initialized", LOG_INFO);
	return nullptr;
}

static void fs_destroy(void *private_data)
{
	Logger::log("Filesystem destroyed", LOG_INFO);
	g_FileSystem.unmount();
}

static int fs_getattr(const char *path, struct stat *st, fuse_file_info *fi)
{
	Logger::log(std::string("getattr called for path: ") + path, LOG_DEBUG);
	std::memset(st, 0, sizeof(struct stat));
	ErrorCode err;
	*st = g_FileSystem.getFileStat(path, err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_flush(const char *path, fuse_file_info *fi)
{
	Logger::log(std::string("flush called for path: ") + path, LOG_DEBUG);
	return 0;
}

static int fs_fsync(const char *path, int isdatasync, fuse_file_info *fi)
{
	Logger::log(std::string("fsync called for path: ") + path, LOG_DEBUG);
	return 0;
}

static int fs_opendir(const char *path, fuse_file_info *fi)
{
	ErrorCode err;
	Logger::log(std::string("opendir called for path: ") + path, LOG_DEBUG);
	struct stat st = g_FileSystem.getFileStat(path, err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	if (!S_ISDIR(st.st_mode))
	{
		return -ENOTDIR;
	}
	fi->fh = st.st_ino;
	return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, fuse_file_info *fi, enum fuse_readdir_flags flags)
{
	ErrorCode err;
	FS &fs = g_FileSystem;
	Logger::log(std::string("readdir called for path: ") + path, LOG_DEBUG);
	Ino inodeNumber = fi->fh;
	int32_t totalEntries = fs.getDirectorySize(inodeNumber, err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	if (offset >= totalEntries)
	{
		return 0;
	}
	while (offset < totalEntries)
	{
		std::vector<DirEntry> entries = fs.listDir(inodeNumber, offset, 1, err);
		if (err != SUCCESS)
		{
			return errorCodeToInt(err);
		}
		if (entries.empty())
		{
			offset += 1;
			continue;
		}
		auto &entry = entries[0];
		struct stat st = entry.st;
		std::string name = char60ToString(entry.raw.d_name);
		if (filler(buf, name.c_str(), &st, offset + 1, static_cast<fuse_fill_dir_flags>(0)) != 0)
		{
			return 0;
		}
		offset += 1;
	}
	return 0;
}

static int fs_releasedir(const char *path, fuse_file_info *fi)
{
	Logger::log(std::string("releasedir called for path: ") + path, LOG_DEBUG);
	return 0;
}

static int fs_fsyncdir(const char *path, int isdatasync, fuse_file_info *fi)
{
	Logger::log(std::string("fsyncdir called for path: ") + path, LOG_DEBUG);
	return 0;
}

static int fs_open(const char *path, fuse_file_info *fi)
{
	Logger::log(std::string("open called for path: ") + path, LOG_DEBUG);
	Ino inodeNumber;
	ErrorCode err = g_FileSystem.openFile(path, inodeNumber, fi->flags);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	fi->fh = inodeNumber;
	return 0;
}

static int fs_release(const char *path, fuse_file_info *fi)
{
	Logger::log(std::string("release called for path: ") + path, LOG_DEBUG);
	ErrorCode err = g_FileSystem.closeFile(fi->fh);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_unlink(const char *path)
{
	Logger::log(std::string("unlink called for path: ") + path, LOG_DEBUG);
	ErrorCode err = g_FileSystem.unlinkFile(path);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, fuse_file_info *fi)
{
	Logger::log(std::string("read called for path: ") + path + ", size: " + std::to_string(size) + ", offset: " + std::to_string(offset), LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err;
	uint32_t bytesRead = fs.readFile(fi->fh, reinterpret_cast<uint8_t*>(buf), static_cast<uint32_t>(offset), static_cast<uint32_t>(size), err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return static_cast<int>(bytesRead);
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, fuse_file_info *fi)
{
	Logger::log(std::string("write called for path: ") + path + ", size: " + std::to_string(size) + ", offset: " + std::to_string(offset), LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err;
	uint32_t bytesWritten = fs.writeFile(fi->fh, reinterpret_cast<const uint8_t*>(buf), static_cast<uint32_t>(offset), static_cast<uint32_t>(size), err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return static_cast<int>(bytesWritten);
}

static int fs_create(const char *path, mode_t mode, fuse_file_info *fi)
{
	Logger::log(std::string("create called for path: ") + path, LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err;
	auto [parentPath, name] = splitPathIntoDirAndBase(path);
	Ino newInodeNumber = fs.createFile(parentPath, name, mode, fuse_get_context()->uid, fuse_get_context()->gid, err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	err = fs.openFile(path, newInodeNumber, O_RDWR);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	fi->fh = newInodeNumber;
	return 0;
}

static int fs_rename(const char *from, const char *to, unsigned int flags)
{
	Logger::log(std::string("rename called from path: ") + from + " to path: " + to, LOG_DEBUG);
	FS &fs = g_FileSystem;
	if (flags & (RENAME_EXCHANGE | RENAME_WHITEOUT))
	{
		return -EINVAL;
	}
	ErrorCode err = fs.renameFile(from, to, (flags & RENAME_NOREPLACE) != 0);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_truncate(const char *path, off_t size, fuse_file_info *fi)
{
	Logger::log(std::string("truncate called for path: ") + path + ", size: " + std::to_string(size), LOG_DEBUG);
	FS &fs = g_FileSystem;
	if (fi != nullptr)
	{
		ErrorCode err = fs.truncateFile(fi->fh, static_cast<uint32_t>(size));
		if (err != SUCCESS)
		{
			return errorCodeToInt(err);
		}
		return 0;
	}
	ErrorCode err = fs.truncateFile(path, static_cast<uint32_t>(size));
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_readlink(const char *path, char *buf, size_t size)
{
	Logger::log(std::string("readlink called for path: ") + path, LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err;
	if (size == 0)
	{
		return -ERANGE;
	}
	std::string linkTarget = fs.readLink(path, err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	uint32_t copySize = std::min(static_cast<uint32_t>(size - 1), static_cast<uint32_t>(linkTarget.size()));
	std::memcpy(buf, linkTarget.c_str(), copySize);
	buf[copySize] = '\0';
	return 0;
}

static int fs_statfs(const char *path, struct statvfs *st)
{
	Logger::log(std::string("statfs called for path: ") + path, LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err;
	struct statvfs fsStat = fs.getFSStat(err);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	*st = fsStat;
	return 0;
}

static int fs_mkdir(const char *path, mode_t mode)
{
	Logger::log(std::string("mkdir called for path: ") + path, LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err = fs.mkdir(path, mode, fuse_get_context()->uid, fuse_get_context()->gid);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_rmdir(const char *path)
{
	Logger::log(std::string("rmdir called for path: ") + path, LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err = fs.rmdir(path);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_link(const char *from, const char *to)
{
	Logger::log(std::string("link called from path: ") + from + " to path: " + to, LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err = fs.linkFile(from, to);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_chmod(const char *path, mode_t mode, fuse_file_info *fi)
{
	Logger::log(std::string("chmod called for path: ") + path + ", mode: " + std::to_string(mode), LOG_DEBUG);
	FS &fs = g_FileSystem;
	ErrorCode err = fs.chmod(path, static_cast<uint16_t>(mode));
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_chown(const char *path, uid_t uid, gid_t gid, fuse_file_info *fi)
{
	Logger::log(std::string("chown called for path: ") + path + ", uid: " + std::to_string(uid) + ", gid: " + std::to_string(gid), LOG_DEBUG);
	FS &fs = g_FileSystem;
	bool updateUID = uid != static_cast<uid_t>(-1);
	bool updateGID = gid != static_cast<gid_t>(-1);
	ErrorCode err = fs.chown(path, static_cast<uint16_t>(uid), static_cast<uint16_t>(gid), updateUID, updateGID);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static int fs_utimens(const char *path, const struct timespec tv[2], fuse_file_info *fi)
{
	Logger::log(std::string("utimens called for path: ") + path, LOG_DEBUG);
	FS &fs = g_FileSystem;
	uint32_t newTimes[2];
	uint32_t &atime = newTimes[0];
	uint32_t &mtime = newTimes[1];
	bool modifyTimes[2] = {true, true};
	bool &modifyAtime = modifyTimes[0];
	bool &modifyMtime = modifyTimes[1];
	if (tv == nullptr)
	{
		uint32_t currentTime = static_cast<uint32_t>(time(nullptr));
		atime = currentTime;
		mtime = currentTime;
	}
	else
	{
		const uint32_t NSEC_MAX = 999999999;
		for (int i = 0; i < 2; i++)
		{
			if (tv[i].tv_nsec == UTIME_NOW)
			{
				newTimes[i] = static_cast<uint32_t>(time(nullptr));
			}
			else if (tv[i].tv_nsec == UTIME_OMIT)
			{
				modifyTimes[i] = false;
			}
			else
			{
				if (tv[i].tv_sec < 0 || tv[i].tv_nsec < 0 || tv[i].tv_nsec > NSEC_MAX)
				{
					return -EINVAL;
				}
				newTimes[i] = static_cast<uint32_t>(tv[i].tv_sec);
			}
		}
	}
	ErrorCode err = fs.utimens(path, atime, mtime, modifyAtime, modifyMtime);
	if (err != SUCCESS)
	{
		return errorCodeToInt(err);
	}
	return 0;
}

static struct fuse_operations makeFsOperations()
{
	struct fuse_operations ops = {};
	ops.init = fs_init;
	ops.destroy = fs_destroy;
	ops.getattr = fs_getattr;
	ops.opendir = fs_opendir;
	ops.readdir = fs_readdir;
	ops.releasedir = fs_releasedir;
	ops.fsyncdir = fs_fsyncdir;
	ops.open = fs_open;
	ops.release = fs_release;
	ops.flush = fs_flush;
	ops.fsync = fs_fsync;
	ops.read = fs_read;
	ops.create = fs_create;
	ops.truncate = fs_truncate;
	ops.rename = fs_rename;
	ops.link = fs_link;
	ops.write = fs_write;
	ops.unlink = fs_unlink;
	ops.readlink = fs_readlink;
	ops.statfs = fs_statfs;
	ops.mkdir = fs_mkdir;
	ops.rmdir = fs_rmdir;
	ops.chmod = fs_chmod;
	ops.chown = fs_chown;
	ops.utimens = fs_utimens;
	return ops;
}

struct MountOptions
{
	char *devicePath = nullptr;
	bool showHelp = false;
};

#define OPTION(t, p) { t, offsetof(MountOptions, p), 1 }

static const struct fuse_opt fs_opts[] = 
{
	OPTION("--device=%s", devicePath),
	OPTION("-h", showHelp),
	OPTION("--help", showHelp),
	FUSE_OPT_END
};

static void showHelp()
{
	printf("Usage: minixfs-fuse --device=<device_path> [FUSE options]\n");
}

int main(int argc, char **argv)
{
	Logger::log("Starting filesystem", LOG_INFO);
	FS &fs = g_FileSystem;
	MountOptions options;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (fuse_opt_parse(&args, &options, fs_opts, nullptr) == -1)
	{
		fuse_opt_free_args(&args);
		return 1;
	}
	if (options.showHelp || options.devicePath == nullptr)
	{
		showHelp();
		fuse_opt_free_args(&args);
		return 0;
	}
	fs.setDevicePath(options.devicePath);
	ErrorCode err = fs.mount();
	if (err != SUCCESS)
	{
		fprintf(stderr, "Failed to mount filesystem. Error code: %d\n", err);
		fuse_opt_free_args(&args);
		return 1;
	}
	struct fuse_operations fs_oper = makeFsOperations();
	int ret = fuse_main(args.argc, args.argv, &fs_oper, nullptr);
	fs.unmount();
	fuse_opt_free_args(&args);
	return ret;
}
