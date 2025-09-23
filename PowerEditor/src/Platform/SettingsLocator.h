#pragma once

#include <string>

namespace npp::platform
{
    struct SettingsLocatorInputs
    {
        std::wstring installDirectory;
        std::wstring applicationSupportDirectory;
        std::wstring roamingDirectory;
        bool useLocalConfiguration = false;
    };

    struct SettingsDirectories
    {
        std::wstring userSettingsRoot;
        std::wstring pluginInstallRoot;
        std::wstring pluginInstallConfig;
        std::wstring userPluginsRoot;
        std::wstring userPluginsConfig;
    };

    SettingsDirectories resolveSettingsDirectories(const SettingsLocatorInputs& inputs);

    SettingsDirectories prepareSettingsDirectories(const std::wstring& installDirectory, bool useLocalConfiguration);
}
