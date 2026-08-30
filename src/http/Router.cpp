#include "Router.hpp"

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

    // Önce tam prefix eşleşmesini doğruluyoruz.
    for (i = 0; i < locationSize; ++i)
    {
        if (path[i] != locationPath[i])
            return false;
    }

    // `/images` ile `/imagesfoo` arasındaki yanlış-pozitif eşleşmeyi burada engelliyoruz.
    // Prefix aynı olsa bile, location path tam bir path segmenti olmalı:
    //   - path == locationPath
    //   - veya path, locationPath + "/" ile devam etmeli
    if (path.size() == locationSize)
        return true;

        // Burada overflow yok mu
    return (path[locationSize] == '/');
}

const LocationConfig* Router::match(const std::string& path, const ServerConfig& config)
{
    const LocationConfig* bestMatch;
    size_t bestLength;
    size_t i;

    // Longest-prefix-match mantığı: önce tüm location'ları dolaş, sonra en uzun eşleşeni seç.
    // Böylece `/` fallback olurken `/images` gibi daha spesifik location'lar öncelik kazanır.
    bestMatch = NULL;
    bestLength = 0;
    for (i = 0; i < config.locations.size(); ++i)
    {
        const LocationConfig& location = config.locations[i];

        if (!matchesLocationPath(path, location.path))
            continue;

        // En uzun eşleşen path, en spesifik location'dır.
        if (location.path.size() > bestLength)
        {
            bestMatch = &config.locations[i];
            bestLength = location.path.size();
        }
    }
    return bestMatch;
}
