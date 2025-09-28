#pragma once

#include "Platform/SystemServices.h"

#include <cstddef>
#include <string>

namespace npp::platform
{
    struct GuiPreferencesSnapshot
    {
        bool rememberLastSession = true;
        bool keepSessionAbsentFileEntries = false;
        bool detectEncoding = true;
        bool saveAllConfirm = true;
        bool checkHistoryFiles = false;
        bool muteSounds = false;
        int trayIconBehavior = 0;
        int multiInstanceBehavior = 0;
        int fileAutoDetectionMask = 0;
        int backupStrategy = 0;
        bool useCustomDirectory = false;
        bool isSnapshotMode = false;
        std::size_t snapshotIntervalMs = 0u;
        std::wstring backupDirectory;
    };

    void persistGuiPreferences(PreferencesStore& store,
        const std::wstring& guiDomain,
        const std::wstring& backupDomain,
        const GuiPreferencesSnapshot& snapshot);

    void backfillGuiPreferences(PreferencesStore& store,
        const std::wstring& guiDomain,
        const std::wstring& backupDomain,
        const GuiPreferencesSnapshot& snapshot);
}

