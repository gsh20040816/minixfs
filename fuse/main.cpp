#define FUSE_USE_VERSION 35
#include "FS.h"
#include "Utils.h"
#include "Inode.h"
#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <cstring>

FS g_FileSystem;

static void *fs_init(fuse_conn_info *conn, fuse_config *cfg)
{
	cfg->kernel_cache = 1;
	return nullptr;
}

static int fs_getattr(const char *path, struct stat *st, fuse_file_info *fi)
{
	std::memset(st, 0, sizeof(struct stat));
	ErrorCode err;
	Attribute attr = g_FileSystem.getFileAttribute(path, err);
	if (err != SUCCESS)
	{
		return -ENOENT;
	}
	*st = g_FileSystem.attrToStat(attr);
	return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, fuse_file_info *fi, enum fuse_readdir_flags flags)
{
	ErrorCode err;
	FS &fs = g_FileSystem;
	Ino dirInodeNumber = fs.getInodeFromPath(path, err);
	if (err != SUCCESS)
	{
		if (err == ERROR_NOT_DIRECTORY)
		{
			return -ENOTDIR;
		}
		else if (err == ERROR_FILE_NOT_FOUND)
		{
			return -ENOENT;
		}
		else
		{	
			return -EIO;
		}
	}
	MinixInode3 dirInode;
	err = fs.readInode(dirInodeNumber, &dirInode);
	if (err != SUCCESS)
	{
		return -EIO;
	}
	if (!dirInode.isDirectory())
	{
		return -ENOTDIR;
	}
	int32_t totalEntries = dirInode.i_size / sizeof(DirEntryOnDisk);
	if (offset >= totalEntries)
	{
		return 0;
	}
	while (offset < totalEntries)
	{
		std::vector<DirEntry> entries = fs.listDir(path, offset, 1, err);
		if (err != SUCCESS)
		{
			if (err == ERROR_NOT_DIRECTORY)
			{
				return -ENOTDIR;
			}
			else if (err == ERROR_FILE_NOT_FOUND)
			{
				return -ENOENT;
			}
			else
			{
				return -EIO;
			}
		}
		if (entries.empty())
		{
			offset += 1;
			continue;
		}
		auto &entry = entries[0];
		struct stat st = fs.attrToStat(entry.attribute);
		std::string name = char60ToString(entry.raw.d_name);
		if (filler(buf, name.c_str(), &st, offset + 1, static_cast<fuse_fill_dir_flags>(0)) != 0)
		{
			return 0;
		}
		offset += 1;
	}
	return 0;
}

static int fs_open(const char *path, fuse_file_info *fi)
{
	if ((fi->flags & O_ACCMODE) != O_RDONLY)
	{
		return -EACCES;
	}
	ErrorCode err;
	Attribute attr = g_FileSystem.getFileAttribute(path, err);
	if (err != SUCCESS)
	{
		if (err == ERROR_FILE_NOT_FOUND)
		{
			return -ENOENT;
		}
		else
		{
			return -EIO;
		}
	}
	if (!S_ISREG(attr.mode))
	{
		return -EISDIR;
	}
	fi->fh = attr.ino;
	return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, fuse_file_info *fi)
{
	FS &fs = g_FileSystem;
	ErrorCode err;
	uint32_t bytesRead = fs.readFile(path, reinterpret_cast<uint8_t*>(buf), static_cast<uint32_t>(offset), static_cast<uint32_t>(size), err);
	if (err != SUCCESS && err != ERROR_READ_FILE_END)
	{
		return -EIO;
	}
	return static_cast<int>(bytesRead);
}

static struct fuse_operations makeFsOperations()
{
	struct fuse_operations ops = {};
	ops.init = fs_init;
	ops.getattr = fs_getattr;
	ops.readdir = fs_readdir;
	ops.open = fs_open;
	ops.read = fs_read;
	return ops;
}

struct MountOptions
{
	char *devicePath;
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
