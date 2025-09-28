#include "catch.hpp"

#include "Platform/PreferencesSync.h"

#include <limits>

namespace
{
    constexpr wchar_t kGuiDomain[] = L"unit.test.gui";
    constexpr wchar_t kBackupDomain[] = L"unit.test.gui.backup";
}

TEST_CASE("persistGuiPreferences writes all values", "[platform][preferences]")
{
    auto& store = npp::platform::SystemServices::instance().preferences();
    store.clearDomain(kGuiDomain);
    store.clearDomain(kBackupDomain);

    npp::platform::GuiPreferencesSnapshot snapshot{};
    snapshot.rememberLastSession = false;
    snapshot.keepSessionAbsentFileEntries = true;
    snapshot.detectEncoding = false;
    snapshot.saveAllConfirm = false;
    snapshot.checkHistoryFiles = true;
    snapshot.muteSounds = true;
    snapshot.trayIconBehavior = 2;
    snapshot.multiInstanceBehavior = 3;
    snapshot.fileAutoDetectionMask = 7;
    snapshot.backupStrategy = 4;
    snapshot.useCustomDirectory = true;
    snapshot.isSnapshotMode = true;
    snapshot.snapshotIntervalMs = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) + 42u;
    snapshot.backupDirectory = L"C:/temp";

    npp::platform::persistGuiPreferences(store, kGuiDomain, kBackupDomain, snapshot);

    auto remember = store.getBoolean(kGuiDomain, L"rememberLastSession");
    REQUIRE(remember.has_value());
    CHECK_FALSE(*remember);

    auto keepAbsent = store.getBoolean(kGuiDomain, L"keepSessionAbsentFileEntries");
    REQUIRE(keepAbsent.has_value());
    CHECK(*keepAbsent);

    auto detectEncoding = store.getBoolean(kGuiDomain, L"detectEncoding");
    REQUIRE(detectEncoding.has_value());
    CHECK_FALSE(*detectEncoding);

    auto saveAllConfirm = store.getBoolean(kGuiDomain, L"saveAllConfirm");
    REQUIRE(saveAllConfirm.has_value());
    CHECK_FALSE(*saveAllConfirm);

    auto checkHistory = store.getBoolean(kGuiDomain, L"checkHistoryFiles");
    REQUIRE(checkHistory.has_value());
    CHECK(*checkHistory);

    auto muteSounds = store.getBoolean(kGuiDomain, L"muteSounds");
    REQUIRE(muteSounds.has_value());
    CHECK(*muteSounds);

    auto trayBehavior = store.getInt64(kGuiDomain, L"trayIconBehavior");
    REQUIRE(trayBehavior.has_value());
    CHECK(*trayBehavior == 2);

    auto multiInstance = store.getInt64(kGuiDomain, L"multiInstanceBehavior");
    REQUIRE(multiInstance.has_value());
    CHECK(*multiInstance == 3);

    auto autodetect = store.getInt64(kGuiDomain, L"fileAutoDetectionMask");
    REQUIRE(autodetect.has_value());
    CHECK(*autodetect == 7);

    auto strategy = store.getInt64(kBackupDomain, L"strategy");
    REQUIRE(strategy.has_value());
    CHECK(*strategy == 4);

    auto useCustom = store.getBoolean(kBackupDomain, L"useCustomDirectory");
    REQUIRE(useCustom.has_value());
    CHECK(*useCustom);

    auto snapshotMode = store.getBoolean(kBackupDomain, L"isSnapshotMode");
    REQUIRE(snapshotMode.has_value());
    CHECK(*snapshotMode);

    auto interval = store.getInt64(kBackupDomain, L"snapshotIntervalMs");
    REQUIRE(interval.has_value());
    CHECK(*interval == std::numeric_limits<std::int64_t>::max());

    auto directory = store.getString(kBackupDomain, L"directory");
    REQUIRE(directory.has_value());
    CHECK(*directory == L"C:/temp");
}

TEST_CASE("persistGuiPreferences removes stale directory entries", "[platform][preferences]")
{
    auto& store = npp::platform::SystemServices::instance().preferences();
    store.clearDomain(kGuiDomain);
    store.clearDomain(kBackupDomain);

    REQUIRE(store.setString(kBackupDomain, L"directory", L"existing"));

    npp::platform::GuiPreferencesSnapshot snapshot{};
    snapshot.useCustomDirectory = false;
    snapshot.backupDirectory.clear();

    npp::platform::persistGuiPreferences(store, kGuiDomain, kBackupDomain, snapshot);

    CHECK_FALSE(store.getString(kBackupDomain, L"directory").has_value());
}

TEST_CASE("backfillGuiPreferences populates missing values only", "[platform][preferences]")
{
    auto& store = npp::platform::SystemServices::instance().preferences();
    store.clearDomain(kGuiDomain);
    store.clearDomain(kBackupDomain);

    REQUIRE(store.setBoolean(kGuiDomain, L"rememberLastSession", true));
    REQUIRE(store.setInt64(kBackupDomain, L"strategy", 9));

    npp::platform::GuiPreferencesSnapshot snapshot{};
    snapshot.rememberLastSession = false;
    snapshot.keepSessionAbsentFileEntries = true;
    snapshot.useCustomDirectory = true;
    snapshot.backupDirectory = L"/Users/test/Backups";

    npp::platform::backfillGuiPreferences(store, kGuiDomain, kBackupDomain, snapshot);

    auto remember = store.getBoolean(kGuiDomain, L"rememberLastSession");
    REQUIRE(remember.has_value());
    CHECK(*remember);

    auto keepAbsent = store.getBoolean(kGuiDomain, L"keepSessionAbsentFileEntries");
    REQUIRE(keepAbsent.has_value());
    CHECK(*keepAbsent);

    auto strategy = store.getInt64(kBackupDomain, L"strategy");
    REQUIRE(strategy.has_value());
    CHECK(*strategy == 9);

    auto directory = store.getString(kBackupDomain, L"directory");
    REQUIRE(directory.has_value());
    CHECK(*directory == L"/Users/test/Backups");
}

TEST_CASE("backfillGuiPreferences skips directory when unused", "[platform][preferences]")
{
    auto& store = npp::platform::SystemServices::instance().preferences();
    store.clearDomain(kGuiDomain);
    store.clearDomain(kBackupDomain);

    npp::platform::GuiPreferencesSnapshot snapshot{};
    snapshot.useCustomDirectory = false;
    snapshot.backupDirectory = L"ignored";

    npp::platform::backfillGuiPreferences(store, kGuiDomain, kBackupDomain, snapshot);

    CHECK_FALSE(store.getString(kBackupDomain, L"directory").has_value());
}

