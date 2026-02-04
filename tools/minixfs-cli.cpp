#include <iostream>
#include "FS.h"
#include "Utils.h"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <minixfs_device>" << std::endl;
		return 1;
	}
	std::string devicePath = argv[1];
	FS filesystem(devicePath);
	ErrorCode err = filesystem.mount();
	if (err != SUCCESS)
	{
		std::cerr << "Failed to mount filesystem. Error code: " << err << std::endl;
		return 1;
	}
	while (true)
	{
		std::cout << "minixfs> ";
		std::string commandLine;
		if (!std::getline(std::cin, commandLine))
		{
			break;
		}
		if (commandLine == "exit" || commandLine == "quit")
		{
			break;
		}
		else if (commandLine.rfind("ls ", 0) == 0)
		{
			std::string path = commandLine.substr(3);
			ErrorCode err;
			std::vector<DirEntry> entries = filesystem.listDir(path, err);
			if (err != SUCCESS)
			{
				std::cout << "Failed to list directory. Error code: " << err << std::endl;
				continue;
			}
			if (entries.empty())
			{
				std::cout << "No entries or directory not found." << std::endl;
			}
			else
			{
				for (const DirEntry &entry : entries)
				{
					std::cout << char60ToString(entry.raw.d_name) << std::endl;
				}
			}
		}
		else if (commandLine.rfind("cat ", 0) == 0)
		{
			std::string path = commandLine.substr(4);
			const uint32_t bufferSize = 4096;
			uint8_t buffer[bufferSize];
			uint32_t offset = 0;
			while (true)
			{
				ErrorCode err;
				uint32_t bytesRead = filesystem.readFile(path, buffer, offset, bufferSize, err);
				if (err != SUCCESS && err != ERROR_READ_FILE_END)
				{
					std::cout << "Failed to read file. Error code: " << err << std::endl;
					break;
				}
				if (err == ERROR_READ_FILE_END || bytesRead == 0)
				{
					break;
				}
				std::cout.write(reinterpret_cast<char*>(buffer), bytesRead);
				offset += bytesRead;
			}
			std::cout << std::endl;
		}
		else
		{
			std::cout << "Unknown command." << std::endl;
		}
	}
}