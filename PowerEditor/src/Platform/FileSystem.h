#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace npp::platform
{
    std::wstring combinePath(const std::wstring& base, const std::wstring& component);

    bool directoryExists(const std::wstring& path);

    bool fileExists(const std::wstring& path);

    bool removeFile(const std::wstring& path);

    std::optional<std::uintmax_t> fileSize(const std::wstring& path);

    bool copyFile(const std::wstring& source, const std::wstring& destination, bool failIfExists);
}
