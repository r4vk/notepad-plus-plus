#include "catch.hpp"

#include "Platform/PreferencesMigrator.h"
#include "Platform/SystemServices.h"

#include "TinyXml/tinyxml.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace
{
    struct TempDirectory
    {
        TempDirectory()
        {
            namespace fs = std::filesystem;
            path = fs::temp_directory_path() / fs::unique_path("npp-pref-migr-%%%%%%%%");
            fs::create_directories(path);
        }

        ~TempDirectory()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        std::filesystem::path path;
    };

    void writeConfigSample(const std::filesystem::path& path)
    {
        TiXmlDocument document;
        document.LinkEndChild(TiXmlDeclaration(L"1.0", L"UTF-8", L""));

        auto* root = document.InsertEndChild(TiXmlElement(L"NotepadPlus"))->ToElement();
        auto* guiConfigs = root->InsertEndChild(TiXmlElement(L"GUIConfigs"))->ToElement();

        {
            TiXmlElement remember(L"GUIConfig");
            remember.SetAttribute(L"name", L"RememberLastSession");
            remember.InsertEndChild(TiXmlText(L"no"));
            guiConfigs->InsertEndChild(remember);
        }

        {
            TiXmlElement keepSession(L"GUIConfig");
            keepSession.SetAttribute(L"name", L"KeepSessionAbsentFileEntries");
            keepSession.InsertEndChild(TiXmlText(L"yes"));
            guiConfigs->InsertEndChild(keepSession);
        }

        {
            TiXmlElement detect(L"GUIConfig");
            detect.SetAttribute(L"name", L"DetectEncoding");
            detect.InsertEndChild(TiXmlText(L"no"));
            guiConfigs->InsertEndChild(detect);
        }

        {
            TiXmlElement saveAll(L"GUIConfig");
            saveAll.SetAttribute(L"name", L"SaveAllConfirm");
            saveAll.InsertEndChild(TiXmlText(L"no"));
            guiConfigs->InsertEndChild(saveAll);
        }

        {
            TiXmlElement history(L"GUIConfig");
            history.SetAttribute(L"name", L"CheckHistoryFiles");
            history.InsertEndChild(TiXmlText(L"yes"));
            guiConfigs->InsertEndChild(history);
        }

        {
            TiXmlElement tray(L"GUIConfig");
            tray.SetAttribute(L"name", L"TrayIcon");
            tray.InsertEndChild(TiXmlText(L"2"));
            guiConfigs->InsertEndChild(tray);
        }

        {
            TiXmlElement autoDetection(L"GUIConfig");
            autoDetection.SetAttribute(L"name", L"Auto-detection");
            autoDetection.InsertEndChild(TiXmlText(L"autoUpdate2End"));
            guiConfigs->InsertEndChild(autoDetection);
        }

        {
            TiXmlElement misc(L"GUIConfig");
            misc.SetAttribute(L"name", L"MISC");
            misc.SetAttribute(L"muteSounds", L"yes");
            guiConfigs->InsertEndChild(misc);
        }

        {
            TiXmlElement multiInst(L"GUIConfig");
            multiInst.SetAttribute(L"name", L"multiInst");
            multiInst.SetAttribute(L"setting", 2);
            guiConfigs->InsertEndChild(multiInst);
        }

        {
            TiXmlElement backup(L"GUIConfig");
            backup.SetAttribute(L"name", L"Backup");
            backup.SetAttribute(L"action", 3);
            backup.SetAttribute(L"useCustumDir", L"yes");
            backup.SetAttribute(L"dir", L"/Users/test/Backups");
            backup.SetAttribute(L"isSnapshotMode", L"yes");
            backup.SetAttribute(L"snapshotBackupTiming", 60000);
            guiConfigs->InsertEndChild(backup);
        }

        REQUIRE(document.SaveFile(path.wstring()));
    }

    void writeRegistrySample(const std::filesystem::path& path)
    {
        const std::wstring content =
            L"Windows Registry Editor Version 5.00\r\n\r\n"
            L"[HKEY_CURRENT_USER\\Software\\Notepad++\\macOSPort\\Preferences\\test.gui]\r\n"
            L"\"rememberLastSession\"=dword:00000001\r\n"
            L"\"trayIconBehavior\"=qword:0000000000000003\r\n"
            L"\"fileAutoDetectionMask\"=qword:0000000000000004\r\n\r\n"
            L"[HKEY_CURRENT_USER\\Software\\Notepad++\\macOSPort\\Preferences\\test.gui.backup]\r\n"
            L"\"useCustomDirectory\"=dword:00000000\r\n"
            L"\"directory\"=\"C:\\\\Temp\\\\Backups\"\r\n";

        std::ofstream stream(path, std::ios::binary);
        REQUIRE(stream.good());

        const unsigned char bom[] = {0xFF, 0xFE};
        stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
        stream.write(reinterpret_cast<const char*>(content.c_str()), static_cast<std::streamsize>(content.size() * sizeof(wchar_t)));
    }
}

TEST_CASE("preferences migrator imports GUI settings from config.xml", "[platform][preferences]")
{
    auto& store = npp::platform::SystemServices::instance().preferences();
    const std::wstring guiDomain = L"test.gui";
    const std::wstring backupDomain = L"test.gui.backup";
    store.clearDomain(guiDomain);
    store.clearDomain(backupDomain);

    TempDirectory tempDir;
    const auto configPath = tempDir.path / "config.xml";
    writeConfigSample(configPath);

    npp::platform::PreferencesMigrationConfig config{};
    config.configXmlPath = configPath;
    config.guiDomain = guiDomain;
    config.backupDomain = backupDomain;
    config.skipExistingValues = true;

    auto report = npp::platform::migratePreferences(config, store);

    REQUIRE(report.configXmlImported);
    CHECK_FALSE(report.registryExportImported);
    CHECK(report.warnings.empty());

    auto remember = store.getBoolean(guiDomain, L"rememberLastSession");
    REQUIRE(remember.has_value());
    CHECK_FALSE(*remember);

    auto keepSession = store.getBoolean(guiDomain, L"keepSessionAbsentFileEntries");
    REQUIRE(keepSession.has_value());
    CHECK(*keepSession);

    auto detectEncoding = store.getBoolean(guiDomain, L"detectEncoding");
    REQUIRE(detectEncoding.has_value());
    CHECK_FALSE(*detectEncoding);

    auto saveAllConfirm = store.getBoolean(guiDomain, L"saveAllConfirm");
    REQUIRE(saveAllConfirm.has_value());
    CHECK_FALSE(*saveAllConfirm);

    auto checkHistory = store.getBoolean(guiDomain, L"checkHistoryFiles");
    REQUIRE(checkHistory.has_value());
    CHECK(*checkHistory);

    auto muteSounds = store.getBoolean(guiDomain, L"muteSounds");
    REQUIRE(muteSounds.has_value());
    CHECK(*muteSounds);

    auto trayBehavior = store.getInt64(guiDomain, L"trayIconBehavior");
    REQUIRE(trayBehavior.has_value());
    CHECK(*trayBehavior == 2);

    auto multiInstance = store.getInt64(guiDomain, L"multiInstanceBehavior");
    REQUIRE(multiInstance.has_value());
    CHECK(*multiInstance == 2);

    auto autodetect = store.getInt64(guiDomain, L"fileAutoDetectionMask");
    REQUIRE(autodetect.has_value());
    CHECK(*autodetect == (0x02 | 0x04 | 0x08));

    auto backupStrategy = store.getInt64(backupDomain, L"strategy");
    REQUIRE(backupStrategy.has_value());
    CHECK(*backupStrategy == 3);

    auto useCustomDir = store.getBoolean(backupDomain, L"useCustomDirectory");
    REQUIRE(useCustomDir.has_value());
    CHECK(*useCustomDir);

    auto snapshotMode = store.getBoolean(backupDomain, L"isSnapshotMode");
    REQUIRE(snapshotMode.has_value());
    CHECK(*snapshotMode);

    auto snapshotInterval = store.getInt64(backupDomain, L"snapshotIntervalMs");
    REQUIRE(snapshotInterval.has_value());
    CHECK(*snapshotInterval == 60000);

    auto backupDir = store.getString(backupDomain, L"directory");
    REQUIRE(backupDir.has_value());
    CHECK(*backupDir == L"/Users/test/Backups");

    store.clearDomain(guiDomain);
    store.clearDomain(backupDomain);
}

TEST_CASE("preferences migrator imports registry export", "[platform][preferences]")
{
    auto& store = npp::platform::SystemServices::instance().preferences();
    const std::wstring guiDomain = L"test.gui";
    const std::wstring backupDomain = L"test.gui.backup";
    store.clearDomain(guiDomain);
    store.clearDomain(backupDomain);

    TempDirectory tempDir;
    const auto regPath = tempDir.path / "preferences.reg";
    writeRegistrySample(regPath);

    npp::platform::PreferencesMigrationConfig config{};
    config.registryExportPath = regPath;
    config.guiDomain = guiDomain;
    config.backupDomain = backupDomain;
    config.skipExistingValues = true;

    auto report = npp::platform::migratePreferences(config, store);

    CHECK_FALSE(report.configXmlImported);
    REQUIRE(report.registryExportImported);
    CHECK(report.warnings.empty());

    auto remember = store.getBoolean(guiDomain, L"rememberLastSession");
    REQUIRE(remember.has_value());
    CHECK(*remember);

    auto trayBehavior = store.getInt64(guiDomain, L"trayIconBehavior");
    REQUIRE(trayBehavior.has_value());
    CHECK(*trayBehavior == 3);

    auto autodetect = store.getInt64(guiDomain, L"fileAutoDetectionMask");
    REQUIRE(autodetect.has_value());
    CHECK(*autodetect == 4);

    auto useCustomDir = store.getBoolean(backupDomain, L"useCustomDirectory");
    REQUIRE(useCustomDir.has_value());
    CHECK_FALSE(*useCustomDir);

    auto directory = store.getString(backupDomain, L"directory");
    REQUIRE(directory.has_value());
    CHECK(*directory == L"C:\\Temp\\Backups");

    store.clearDomain(guiDomain);
    store.clearDomain(backupDomain);
}

TEST_CASE("preferences migrator respects skipExisting flag", "[platform][preferences]")
{
    auto& store = npp::platform::SystemServices::instance().preferences();
    const std::wstring guiDomain = L"test.gui";
    store.clearDomain(guiDomain);

    REQUIRE(store.setBoolean(guiDomain, L"rememberLastSession", false));

    TempDirectory tempDir;
    const auto regPath = tempDir.path / "preferences.reg";
    const std::wstring regContent =
        L"Windows Registry Editor Version 5.00\r\n\r\n"
        L"[HKEY_CURRENT_USER\\Software\\Notepad++\\macOSPort\\Preferences\\test.gui]\r\n"
        L"\"rememberLastSession\"=dword:00000001\r\n";

    std::ofstream stream(regPath, std::ios::binary);
    REQUIRE(stream.good());
    const unsigned char bom[] = {0xFF, 0xFE};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    stream.write(reinterpret_cast<const char*>(regContent.c_str()), static_cast<std::streamsize>(regContent.size() * sizeof(wchar_t)));

    npp::platform::PreferencesMigrationConfig config{};
    config.registryExportPath = regPath;
    config.guiDomain = guiDomain;
    config.backupDomain = L"test.gui.backup";
    config.skipExistingValues = true;

    auto report = npp::platform::migratePreferences(config, store);
    REQUIRE(report.registryExportImported);

    auto remember = store.getBoolean(guiDomain, L"rememberLastSession");
    REQUIRE(remember.has_value());
    CHECK_FALSE(*remember);

    store.clearDomain(guiDomain);
}
