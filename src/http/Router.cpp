#include "Router.hpp"

// Verilen path'in, bir location path'inin segment sınırına uyan bir prefix'i olup olmadığını kontrol eder.
bool Router::matchesLocationPath(const std::string& path, const std::string& locationPath)
{
    size_t i;
    size_t locationSize;

    if (locationPath == "/")
        return true;

    if (locationPath.empty())
        return false;

    locationSize = locationPath.size();
    if (path.size() < locationSize)
        return false;

    for (i = 0; i < locationSize; ++i)
    {
        if (path[i] != locationPath[i])
            return false;
    }

    if (path.size() == locationSize)
        return true;

    return (path[locationSize] == '/');
}

// Verilen path için config'teki location'lar arasından en uzun (en spesifik) eşleşeni döner.
const LocationConfig* Router::match(const std::string& path, const ServerConfig& config)
{
    const LocationConfig* bestMatch;
    size_t bestLength;
    size_t i;

    bestMatch = NULL;
    bestLength = 0;
    for (i = 0; i < config.locations.size(); ++i)
    {
        const LocationConfig& location = config.locations[i];

        if (!matchesLocationPath(path, location.path))
            continue;

        if (location.path.size() > bestLength)
        {
            bestMatch = &config.locations[i];
            bestLength = location.path.size();
        }
    }
    return bestMatch;
}
