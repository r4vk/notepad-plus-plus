#pragma once

#include <string>
#include <vector>

namespace npp::platform
{
    struct DisplayAdapterInfo
    {
        std::wstring description;
        std::wstring driverVersion;
    };

    struct DisplayMetrics
    {
        int primaryWidth = 0;
        int primaryHeight = 0;
        int primaryScalePercent = 0;
        int monitorCount = 0;
        std::vector<DisplayAdapterInfo> adapters;
        bool adaptersTruncated = false;
    };

    struct OperatingSystemInfo
    {
        std::wstring name;
        std::wstring version;
        std::wstring build;
        std::wstring architecture;
    };

    struct SystemProfile
    {
        DisplayMetrics display;
        OperatingSystemInfo operatingSystem;
    };

    SystemProfile querySystemProfile();
}

