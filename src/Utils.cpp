#include "Utils.h"

std::vector<std::string> splitPath(const std::string &path)
{
	std::vector<std::string> components;
	size_t start = 0;
	std::string component;
	while (start < path.length())
	{
		size_t end = path.find('/', start);
		if (end == std::string::npos)
		{
			end = path.length();
		}
		component = path.substr(start, end - start);
		if (!component.empty())
		{
			components.push_back(component);
		}
		start = end + 1;
	}
	return components;
}