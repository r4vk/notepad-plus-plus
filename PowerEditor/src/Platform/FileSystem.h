#pragma once

#include <string>

namespace npp::platform
{
    std::wstring combinePath(const std::wstring& base, const std::wstring& component);

    bool directoryExists(const std::wstring& path);

    bool fileExists(const std::wstring& path);

    bool removeFile(const std::wstring& path);
}
