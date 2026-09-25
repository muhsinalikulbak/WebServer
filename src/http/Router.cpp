#include "Router.hpp"

// Checks whether the given path has a prefix matching a location path at a segment boundary.
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

// Returns the longest (most specific) matching location for the given path.
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
