#include "FileUtils.hpp"
#include <sys/stat.h>
#include <fstream>
#include <sstream>

// Checks with stat() whether the path exists on disk, whether it is a file or directory.
bool FileUtils::pathExists(const std::string& path)
{
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

// Checks whether the given path is a directory; returns false if stat fails.
bool FileUtils::isDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

// Reads the entire file in binary mode into outContent; returns false if the file cannot be opened.
bool FileUtils::readFile(const std::string& path, std::string& outContent)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    outContent = ss.str();
    return true;
}

// Joins two path components with exactly one "/" between them.
std::string FileUtils::joinPath(const std::string& base, const std::string& rest)
{
    std::string result = base;
    bool baseEndsSlash = !result.empty() && result[result.length() - 1] == '/';
    bool restStartsSlash = !rest.empty() && rest[0] == '/';

    if (baseEndsSlash && restStartsSlash)
        result.erase(result.length() - 1);
    else if (!baseEndsSlash && !restStartsSlash)
        result += "/";

    return result + rest;
}

// Returns the part of the path after the last '/'; returns the entire path if there is no '/'.
std::string FileUtils::lastPathSegment(const std::string& path)
{
    size_t slashPos = path.find_last_of('/');
    if (slashPos == std::string::npos)
        return path;
    return path.substr(slashPos + 1);
}

// Appends '/' to the path if it does not already end with one or is empty.
std::string FileUtils::withTrailingSlash(const std::string& path)
{
    if (path.empty() || path[path.length() - 1] != '/')
        return path + "/";
    return path;
}
