#include "FileUtils.hpp"
#include <sys/stat.h>
#include <fstream>
#include <sstream>

// stat() ile path'in diskte var olup olmadığını kontrol eder (dosya ya da dizin fark etmez).
bool FileUtils::pathExists(const std::string& path)
{
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

// Verilen path'in bir dizin olup olmadığını kontrol eder; stat başarısız olursa false döner.
bool FileUtils::isDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

// Dosya içeriğini binary modda tek seferde okuyup outContent'e yazar; açılamazsa false döner.
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

// İki path parçasını, aralarında tam olarak bir "/" kalacak şekilde birleştirir.
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

// Path'in son '/' işaretinden sonraki parçasını döner; '/' yoksa path'in tamamını döner.
std::string FileUtils::lastPathSegment(const std::string& path)
{
    size_t slashPos = path.find_last_of('/');
    if (slashPos == std::string::npos)
        return path;
    return path.substr(slashPos + 1);
}

// Path sonu '/' ile bitmiyorsa (veya boşsa) sona '/' ekler.
std::string FileUtils::withTrailingSlash(const std::string& path)
{
    if (path.empty() || path[path.length() - 1] != '/')
        return path + "/";
    return path;
}
