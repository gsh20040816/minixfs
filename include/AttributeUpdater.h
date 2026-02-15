#pragma once

#include "InodeReader.h"
#include "InodeWriter.h"

struct AttributeUpdater
{
	InodeReader *inodeReader;
	InodeWriter *inodeWriter;
	void setInodeReader(InodeReader &inodeReader);
	void setInodeWriter(InodeWriter &inodeWriter);
	ErrorCode chmod(Ino inodeNumber, uint16_t mode);
	ErrorCode chown(Ino inodeNumber, uint16_t uid, uint16_t gid);
	ErrorCode utimens(Ino inodeNumber, uint32_t atime, uint32_t mtime);
};