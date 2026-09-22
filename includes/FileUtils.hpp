// FileUtils.hpp
#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

namespace FileUtils
{
    bool pathExists(const std::string& path);
    bool isDirectory(const std::string& path);
    bool readFile(const std::string& path, std::string& outContent);
    std::string joinPath(const std::string& base, const std::string& rest);
}

#endif