#pragma once

#include <string>

namespace npp::platform
{
    enum class KnownDirectory
    {
        UserHome,
        RoamingData,
        LocalData,
        ProgramFiles,
        ApplicationSupport
    };

    std::wstring pathFor(KnownDirectory directory);

    bool ensureDirectoryExists(const std::wstring& path);
}
