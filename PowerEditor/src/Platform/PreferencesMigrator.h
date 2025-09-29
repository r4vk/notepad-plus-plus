#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace npp::platform
{
    class PreferencesStore;

    struct PreferencesMigrationConfig
    {
        std::optional<std::filesystem::path> configXmlPath;
        std::optional<std::filesystem::path> registryExportPath;
        std::wstring guiDomain;
        std::wstring backupDomain;
        bool skipExistingValues = true;
    };

    struct PreferencesMigrationReport
    {
        bool configXmlImported = false;
        bool registryExportImported = false;
        std::vector<std::wstring> warnings;
    };

    PreferencesMigrationReport migratePreferences(const PreferencesMigrationConfig& config,
        PreferencesStore& store);
}
