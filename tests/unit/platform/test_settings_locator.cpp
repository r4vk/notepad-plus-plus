#include "catch.hpp"

#include "Platform/FileSystem.h"
#include "Platform/SettingsLocator.h"

#include <string>

namespace
{
    std::wstring combine(const std::wstring& base, const std::wstring& child)
    {
        return npp::platform::combinePath(base, child);
    }
}

TEST_CASE("Local configuration keeps settings within the install directory")
{
    npp::platform::SettingsLocatorInputs inputs{};
    inputs.installDirectory = L"/Applications/Notepad++";
    inputs.useLocalConfiguration = true;

    const auto directories = npp::platform::resolveSettingsDirectories(inputs);

    REQUIRE(directories.userSettingsRoot == inputs.installDirectory);
    REQUIRE(directories.pluginInstallRoot == combine(inputs.installDirectory, L"plugins"));
    REQUIRE(directories.pluginInstallConfig == combine(directories.pluginInstallRoot, L"Config"));
    REQUIRE(directories.userPluginsRoot == directories.pluginInstallRoot);
    REQUIRE(directories.userPluginsConfig == directories.pluginInstallConfig);
}

TEST_CASE("Application Support path drives per-user directories when available")
{
    npp::platform::SettingsLocatorInputs inputs{};
    inputs.installDirectory = L"/Applications/Notepad++";
    inputs.applicationSupportDirectory = L"/Users/dev/Library/Application Support";

    const auto directories = npp::platform::resolveSettingsDirectories(inputs);
    const auto expectedUserRoot = combine(inputs.applicationSupportDirectory, L"Notepad++");

    REQUIRE(directories.userSettingsRoot == expectedUserRoot);

#ifndef _WIN32
    REQUIRE(directories.pluginInstallRoot == combine(expectedUserRoot, L"plugins"));
#else
    REQUIRE(directories.pluginInstallRoot == combine(inputs.installDirectory, L"plugins"));
#endif

    const auto expectedUserPlugins = combine(expectedUserRoot, L"plugins");
    REQUIRE(directories.userPluginsRoot == expectedUserPlugins);
    REQUIRE(directories.userPluginsConfig == combine(expectedUserPlugins, L"Config"));
}

TEST_CASE("Roaming directory is used when Application Support is unavailable")
{
    npp::platform::SettingsLocatorInputs inputs{};
    inputs.installDirectory = L"/Applications/Notepad++";
    inputs.roamingDirectory = L"/Users/dev/.config";

    const auto directories = npp::platform::resolveSettingsDirectories(inputs);
    const auto expectedUserRoot = combine(inputs.roamingDirectory, L"Notepad++");

    REQUIRE(directories.userSettingsRoot == expectedUserRoot);
    REQUIRE(directories.userPluginsRoot == combine(expectedUserRoot, L"plugins"));
    REQUIRE(directories.userPluginsConfig == combine(directories.userPluginsRoot, L"Config"));
}

TEST_CASE("Install directory is the fallback when no user base is provided")
{
    npp::platform::SettingsLocatorInputs inputs{};
    inputs.installDirectory = L"/Applications/Notepad++";

    const auto directories = npp::platform::resolveSettingsDirectories(inputs);

    REQUIRE(directories.userSettingsRoot == inputs.installDirectory);
    REQUIRE(directories.userPluginsRoot == combine(inputs.installDirectory, L"plugins"));
    REQUIRE(directories.userPluginsConfig == combine(directories.userPluginsRoot, L"Config"));
}
