#include "Platform/PreferencesSync.h"

#include <limits>

namespace npp::platform
{
    namespace
    {
        std::int64_t clampSnapshotInterval(std::size_t value)
        {
            constexpr auto maxInt64 = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());
            if (value > maxInt64)
            {
                return std::numeric_limits<std::int64_t>::max();
            }

            return static_cast<std::int64_t>(value);
        }

        void writeGuiPreferences(PreferencesStore& store,
            const std::wstring& guiDomain,
            const std::wstring& backupDomain,
            const GuiPreferencesSnapshot& snapshot,
            bool onlyWhenMissing)
        {
            const auto maybeWriteBool = [&](const std::wstring& domain, const std::wstring& key, bool value)
            {
                if (!onlyWhenMissing || !store.getBoolean(domain, key).has_value())
                {
                    store.setBoolean(domain, key, value);
                }
            };

            const auto maybeWriteInt = [&](const std::wstring& domain, const std::wstring& key, std::int64_t value)
            {
                if (!onlyWhenMissing || !store.getInt64(domain, key).has_value())
                {
                    store.setInt64(domain, key, value);
                }
            };

            const auto maybeWriteString = [&](const std::wstring& domain, const std::wstring& key, const std::wstring& value)
            {
                if (!onlyWhenMissing || !store.getString(domain, key).has_value())
                {
                    store.setString(domain, key, value);
                }
            };

            maybeWriteBool(guiDomain, L"rememberLastSession", snapshot.rememberLastSession);
            maybeWriteBool(guiDomain, L"keepSessionAbsentFileEntries", snapshot.keepSessionAbsentFileEntries);
            maybeWriteBool(guiDomain, L"detectEncoding", snapshot.detectEncoding);
            maybeWriteBool(guiDomain, L"saveAllConfirm", snapshot.saveAllConfirm);
            maybeWriteBool(guiDomain, L"checkHistoryFiles", snapshot.checkHistoryFiles);
            maybeWriteBool(guiDomain, L"muteSounds", snapshot.muteSounds);
            maybeWriteInt(guiDomain, L"trayIconBehavior", snapshot.trayIconBehavior);
            maybeWriteInt(guiDomain, L"multiInstanceBehavior", snapshot.multiInstanceBehavior);
            maybeWriteInt(guiDomain, L"fileAutoDetectionMask", snapshot.fileAutoDetectionMask);

            maybeWriteInt(backupDomain, L"strategy", snapshot.backupStrategy);
            maybeWriteBool(backupDomain, L"useCustomDirectory", snapshot.useCustomDirectory);
            maybeWriteBool(backupDomain, L"isSnapshotMode", snapshot.isSnapshotMode);
            maybeWriteInt(backupDomain, L"snapshotIntervalMs", clampSnapshotInterval(snapshot.snapshotIntervalMs));

            if (snapshot.useCustomDirectory && !snapshot.backupDirectory.empty())
            {
                maybeWriteString(backupDomain, L"directory", snapshot.backupDirectory);
            }
            else if (!onlyWhenMissing)
            {
                store.remove(backupDomain, L"directory");
            }
        }
    }

    void persistGuiPreferences(PreferencesStore& store,
        const std::wstring& guiDomain,
        const std::wstring& backupDomain,
        const GuiPreferencesSnapshot& snapshot)
    {
        writeGuiPreferences(store, guiDomain, backupDomain, snapshot, false);
    }

    void backfillGuiPreferences(PreferencesStore& store,
        const std::wstring& guiDomain,
        const std::wstring& backupDomain,
        const GuiPreferencesSnapshot& snapshot)
    {
        writeGuiPreferences(store, guiDomain, backupDomain, snapshot, true);
    }
}

