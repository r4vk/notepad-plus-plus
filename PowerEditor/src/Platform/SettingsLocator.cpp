#include "Platform/SettingsLocator.h"

#include "Platform/FileSystem.h"
#include "Platform/PathProvider.h"

namespace npp::platform
{
    namespace
    {
        bool hasValue(const std::wstring& value)
        {
            return !value.empty();
        }
    }

    SettingsDirectories resolveSettingsDirectories(const SettingsLocatorInputs& inputs)
    {
        SettingsDirectories directories{};

        if (!hasValue(inputs.installDirectory))
        {
            return directories;
        }

        if (inputs.useLocalConfiguration)
        {
            directories.userSettingsRoot = inputs.installDirectory;
            directories.pluginInstallRoot = combinePath(inputs.installDirectory, L"plugins");
            directories.pluginInstallConfig = combinePath(directories.pluginInstallRoot, L"Config");
            directories.userPluginsRoot = directories.pluginInstallRoot;
            directories.userPluginsConfig = directories.pluginInstallConfig;
            return directories;
        }

        std::wstring userBase;
        if (hasValue(inputs.applicationSupportDirectory))
        {
            userBase = inputs.applicationSupportDirectory;
        }
        else if (hasValue(inputs.roamingDirectory))
        {
            userBase = inputs.roamingDirectory;
        }

        if (hasValue(userBase))
        {
            directories.userSettingsRoot = combinePath(userBase, L"Notepad++");
            directories.userPluginsRoot = combinePath(directories.userSettingsRoot, L"plugins");
        }
        else
        {
            directories.userSettingsRoot = inputs.installDirectory;
            directories.userPluginsRoot = combinePath(inputs.installDirectory, L"plugins");
        }

    #ifdef _WIN32
        directories.pluginInstallRoot = combinePath(inputs.installDirectory, L"plugins");
    #else
        if (hasValue(userBase))
        {
            directories.pluginInstallRoot = combinePath(directories.userSettingsRoot, L"plugins");
        }
        else
        {
            directories.pluginInstallRoot = combinePath(inputs.installDirectory, L"plugins");
        }
    #endif

        directories.pluginInstallConfig = combinePath(directories.pluginInstallRoot, L"Config");

        if (hasValue(userBase))
        {
            directories.userPluginsConfig = combinePath(directories.userPluginsRoot, L"Config");
        }
        else
        {
            directories.userPluginsConfig = directories.pluginInstallConfig;
        }

        return directories;
    }

    SettingsDirectories prepareSettingsDirectories(const std::wstring& installDirectory, bool useLocalConfiguration)
    {
        SettingsLocatorInputs inputs{};
        inputs.installDirectory = installDirectory;
        inputs.applicationSupportDirectory = pathFor(KnownDirectory::ApplicationSupport);
        inputs.roamingDirectory = pathFor(KnownDirectory::RoamingData);
        inputs.useLocalConfiguration = useLocalConfiguration;

        return resolveSettingsDirectories(inputs);
    }
}
