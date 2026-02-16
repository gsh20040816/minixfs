#include "SymlinkCreator.h"

void SymlinkCreator::setFileCreator(FileCreator &fileCreator)
{
	this->fileCreator = &fileCreator;
}

void SymlinkCreator::setFileWriter(FileWriter &fileWriter)
{
	this->fileWriter = &fileWriter;
}

Ino SymlinkCreator::createSymlink(Ino parentInodeNumber, const std::string &name, const std::string &targetPath, uint16_t mod, uint16_t uid, uint16_t gid, ErrorCode &outError)
{
	Ino symlinkInodeNumber = fileCreator->createFile(parentInodeNumber, name, mod | S_IFLNK, uid, gid, outError);
	if (outError != SUCCESS)
	{
		return 0;
	}
	std::vector<uint8_t> targetPathData(targetPath.begin(), targetPath.end());
	ErrorCode err = fileWriter->writeFile(symlinkInodeNumber, targetPathData.data(), 0, targetPathData.size());
	if (err != SUCCESS)
	{
		outError = err;
		return 0;
	}
	return symlinkInodeNumber;
}