#pragma once

#include "FileReader.h"
#include "InodeReader.h"

struct LinkReader
{
	FileReader *fileReader;
	InodeReader *inodeReader;
	void setFileReader(FileReader &fileReader);
	void setInodeReader(InodeReader &inodeReader);
	ErrorCode readLink(Ino inodeNumber, std::string &outLinkTarget);
};