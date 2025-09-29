#include "Platform/PreferencesMigrator.h"

#include "Platform/PreferencesSync.h"

#include "TinyXml/tinyxml.h"

#include <algorithm>
#include <codecvt>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace npp::platform
{
    namespace
    {
        constexpr int kChangeDetectDisabled = 0x00;
        constexpr int kChangeDetectEnabledOld = 0x01;
        constexpr int kChangeDetectEnabledNew = 0x02;
        constexpr int kChangeDetectAutoUpdate = 0x04;
        constexpr int kChangeDetectGoToEnd = 0x08;

        constexpr std::wstring_view kRegistryRoot = L"HKEY_CURRENT_USER\\Software\\Notepad++\\macOSPort\\Preferences";

        bool equalsIgnoreCase(std::wstring_view text, std::wstring_view candidate)
        {
            if (text.size() != candidate.size())
            {
                return false;
            }

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (std::towlower(text[i]) != std::towlower(candidate[i]))
                {
                    return false;
                }
            }

            return true;
        }

        bool parseYesNo(std::wstring_view value, bool fallback)
        {
            if (equalsIgnoreCase(value, L"yes") || equalsIgnoreCase(value, L"true") || value == L"1")
            {
                return true;
            }

            if (equalsIgnoreCase(value, L"no") || equalsIgnoreCase(value, L"false") || value == L"0")
            {
                return false;
            }

            return fallback;
        }

        std::optional<std::wstring> nodeText(const TiXmlElement& element)
        {
            if (const TiXmlNode* node = element.FirstChild())
            {
                if (const wchar_t* raw = node->Value())
                {
                    return std::wstring(raw);
                }
            }

            return std::nullopt;
        }

        int parseAutoDetectionMask(std::wstring_view value)
        {
            if (value == L"yesOld")
                return kChangeDetectEnabledOld;
            if (value == L"autoOld")
                return kChangeDetectEnabledOld | kChangeDetectAutoUpdate;
            if (value == L"Update2EndOld")
                return kChangeDetectEnabledOld | kChangeDetectGoToEnd;
            if (value == L"autoUpdate2EndOld")
                return kChangeDetectEnabledOld | kChangeDetectAutoUpdate | kChangeDetectGoToEnd;
            if (value == L"yes")
                return kChangeDetectEnabledNew;
            if (value == L"auto")
                return kChangeDetectEnabledNew | kChangeDetectAutoUpdate;
            if (value == L"Update2End")
                return kChangeDetectEnabledNew | kChangeDetectGoToEnd;
            if (value == L"autoUpdate2End")
                return kChangeDetectEnabledNew | kChangeDetectAutoUpdate | kChangeDetectGoToEnd;

            return kChangeDetectDisabled;
        }

        int parseTrayIconBehavior(std::wstring_view value)
        {
            if (value == L"3")
                return 3;
            if (value == L"2")
                return 2;
            if (equalsIgnoreCase(value, L"yes") || value == L"1")
                return 1;
            return 0;
        }

        std::optional<GuiPreferencesSnapshot> readGuiPreferencesFromConfig(
            const std::filesystem::path& path,
            std::vector<std::wstring>& warnings)
        {
            const std::wstring configPath = path.wstring();
            TiXmlDocument document(configPath);
            if (!document.LoadFile())
            {
                warnings.emplace_back(L"Nie udało się wczytać pliku konfiguracyjnego: " + configPath);
                return std::nullopt;
            }

            const TiXmlNode* root = document.FirstChild(L"NotepadPlus");
            if (!root)
            {
                warnings.emplace_back(L"Brak sekcji NotepadPlus w pliku config.xml: " + configPath);
                return std::nullopt;
            }

            const TiXmlElement* guiConfigs = root->FirstChildElement(L"GUIConfigs");
            if (!guiConfigs)
            {
                warnings.emplace_back(L"Brak sekcji GUIConfigs w pliku config.xml: " + configPath);
                return std::nullopt;
            }

            GuiPreferencesSnapshot snapshot{};

            for (const TiXmlElement* element = guiConfigs->FirstChildElement(L"GUIConfig");
                element;
                element = element->NextSiblingElement(L"GUIConfig"))
            {
                const wchar_t* nameRaw = element->Attribute(L"name");
                if (!nameRaw)
                {
                    continue;
                }

                const std::wstring_view name{nameRaw};

                if (name == L"RememberLastSession")
                {
                    if (const auto text = nodeText(*element))
                    {
                        snapshot.rememberLastSession = parseYesNo(*text, snapshot.rememberLastSession);
                    }
                }
                else if (name == L"KeepSessionAbsentFileEntries")
                {
                    if (const auto text = nodeText(*element))
                    {
                        snapshot.keepSessionAbsentFileEntries = parseYesNo(*text, snapshot.keepSessionAbsentFileEntries);
                    }
                }
                else if (name == L"DetectEncoding")
                {
                    if (const auto text = nodeText(*element))
                    {
                        snapshot.detectEncoding = parseYesNo(*text, snapshot.detectEncoding);
                    }
                }
                else if (name == L"SaveAllConfirm")
                {
                    if (const auto text = nodeText(*element))
                    {
                        snapshot.saveAllConfirm = parseYesNo(*text, snapshot.saveAllConfirm);
                    }
                }
                else if (name == L"CheckHistoryFiles")
                {
                    if (const auto text = nodeText(*element))
                    {
                        snapshot.checkHistoryFiles = parseYesNo(*text, snapshot.checkHistoryFiles);
                    }
                }
                else if (name == L"TrayIcon")
                {
                    if (const auto text = nodeText(*element))
                    {
                        snapshot.trayIconBehavior = parseTrayIconBehavior(*text);
                    }
                }
                else if (name == L"Auto-detection")
                {
                    if (const auto text = nodeText(*element))
                    {
                        snapshot.fileAutoDetectionMask = parseAutoDetectionMask(*text);
                    }
                }
                else if (name == L"MISC")
                {
                    if (const wchar_t* muteAttr = element->Attribute(L"muteSounds"))
                    {
                        snapshot.muteSounds = parseYesNo(muteAttr, snapshot.muteSounds);
                    }
                }
                else if (name == L"multiInst")
                {
                    int setting = snapshot.multiInstanceBehavior;
                    if (element->Attribute(L"setting", &setting))
                    {
                        if (setting < 0)
                            setting = 0;
                        if (setting > 2)
                            setting = 2;
                        snapshot.multiInstanceBehavior = setting;
                    }
                }
                else if (name == L"Backup")
                {
                    int action = snapshot.backupStrategy;
                    if (element->Attribute(L"action", &action))
                    {
                        if (action < 0)
                            action = 0;
                        snapshot.backupStrategy = action;
                    }

                    if (const wchar_t* useCustom = element->Attribute(L"useCustumDir"))
                    {
                        snapshot.useCustomDirectory = parseYesNo(useCustom, snapshot.useCustomDirectory);
                    }
                    else if (const wchar_t* useCustomAlt = element->Attribute(L"useCustomDir"))
                    {
                        snapshot.useCustomDirectory = parseYesNo(useCustomAlt, snapshot.useCustomDirectory);
                    }

                    if (const wchar_t* directory = element->Attribute(L"dir"))
                    {
                        snapshot.backupDirectory.assign(directory);
                    }

                    if (const wchar_t* snapshotMode = element->Attribute(L"isSnapshotMode"))
                    {
                        snapshot.isSnapshotMode = parseYesNo(snapshotMode, snapshot.isSnapshotMode);
                    }

                    int snapshotTiming = static_cast<int>(snapshot.snapshotIntervalMs);
                    if (element->Attribute(L"snapshotBackupTiming", &snapshotTiming))
                    {
                        if (snapshotTiming < 0)
                            snapshotTiming = 0;
                        snapshot.snapshotIntervalMs = static_cast<std::size_t>(snapshotTiming);
                    }
                }
            }

            return snapshot;
        }

        std::optional<std::wstring> loadRegistryExport(
            const std::filesystem::path& path,
            std::vector<std::wstring>& warnings)
        {
            const std::wstring registryPath = path.wstring();
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                warnings.emplace_back(L"Nie udało się otworzyć pliku rejestru: " + registryPath);
                return std::nullopt;
            }

            std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            if (bytes.empty())
            {
                return std::wstring();
            }

            if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE)
            {
                const wchar_t* wide = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
                const std::size_t length = (bytes.size() - 2) / sizeof(wchar_t);
                return std::wstring(wide, wide + length);
            }

            std::string utf8(bytes.begin(), bytes.end());
            if (utf8.size() >= 3 && static_cast<unsigned char>(utf8[0]) == 0xEF && static_cast<unsigned char>(utf8[1]) == 0xBB && static_cast<unsigned char>(utf8[2]) == 0xBF)
            {
                utf8.erase(0, 3);
            }

            try
            {
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                return converter.from_bytes(utf8);
            }
            catch (const std::range_error&)
            {
                warnings.emplace_back(L"Nie udało się zdekodować pliku rejestru jako UTF-8: " + registryPath);
            }

            return std::nullopt;
        }

        std::wstring decodeQuotedValue(std::wstring_view quoted)
        {
            std::wstring result;
            if (quoted.empty() || quoted.front() != L'"')
            {
                return result;
            }

            for (std::size_t i = 1; i < quoted.size(); ++i)
            {
                const wchar_t ch = quoted[i];
                if (ch == L'"')
                {
                    break;
                }

                if (ch == L'\\' && i + 1 < quoted.size())
                {
                    const wchar_t next = quoted[++i];
                    switch (next)
                    {
                        case L'\\': result.push_back(L'\\'); break;
                        case L'"': result.push_back(L'"'); break;
                        case L'0': result.push_back(L'\0'); break;
                        case L'n': result.push_back(L'\n'); break;
                        case L'r': result.push_back(L'\r'); break;
                        case L't': result.push_back(L'\t'); break;
                        default: result.push_back(next); break;
                    }
                    continue;
                }

                result.push_back(ch);
            }

            return result;
        }

        std::wstring trim(std::wstring_view view)
        {
            std::size_t start = 0;
            while (start < view.size() && (view[start] == L' ' || view[start] == L'\t'))
            {
                ++start;
            }

            std::size_t end = view.size();
            while (end > start && (view[end - 1] == L' ' || view[end - 1] == L'\t'))
            {
                --end;
            }

            return std::wstring(view.substr(start, end - start));
        }

        struct RegistryLine
        {
            std::wstring domain;
            std::wstring key;
            enum class Type
            {
                String,
                DWord,
                QWord,
                Delete,
                Unsupported
            } type = Type::Unsupported;
            std::wstring rawValue;
        };

        std::optional<RegistryLine> parseRegistryLine(std::wstring_view line)
        {
            const std::size_t equals = line.find(L'=');
            if (equals == std::wstring_view::npos)
            {
                return std::nullopt;
            }

            std::wstring_view keyView = line.substr(0, equals);
            std::wstring_view valueView = line.substr(equals + 1);

            if (keyView.empty() || keyView.front() != L'"')
            {
                return std::nullopt;
            }

            const std::size_t closingQuote = keyView.find(L'"', 1);
            if (closingQuote == std::wstring_view::npos)
            {
                return std::nullopt;
            }

            RegistryLine parsed;
            parsed.key = std::wstring(keyView.substr(1, closingQuote - 1));
            parsed.rawValue = trim(valueView);

            if (parsed.rawValue == L"-")
            {
                parsed.type = RegistryLine::Type::Delete;
                return parsed;
            }

            if (!parsed.rawValue.empty() && parsed.rawValue.front() == L'"')
            {
                parsed.type = RegistryLine::Type::String;
                return parsed;
            }

            if (parsed.rawValue.rfind(L"dword:", 0) == 0)
            {
                parsed.type = RegistryLine::Type::DWord;
                parsed.rawValue.erase(0, 6);
                return parsed;
            }

            if (parsed.rawValue.rfind(L"qword:", 0) == 0)
            {
                parsed.type = RegistryLine::Type::QWord;
                parsed.rawValue.erase(0, 6);
                return parsed;
            }

            return parsed;
        }

        bool applyString(PreferencesStore& store, const std::wstring& domain, const std::wstring& key,
            const std::wstring& value, bool skipExisting)
        {
            if (skipExisting && store.getString(domain, key).has_value())
            {
                return true;
            }

            return store.setString(domain, key, value);
        }

        bool applyBoolean(PreferencesStore& store, const std::wstring& domain, const std::wstring& key,
            std::uint32_t value, bool skipExisting)
        {
            if (skipExisting && store.getBoolean(domain, key).has_value())
            {
                return true;
            }

            return store.setBoolean(domain, key, value != 0u);
        }

        bool applyInt64(PreferencesStore& store, const std::wstring& domain, const std::wstring& key,
            std::int64_t value, bool skipExisting)
        {
            if (skipExisting && store.getInt64(domain, key).has_value())
            {
                return true;
            }

            return store.setInt64(domain, key, value);
        }

        struct MigrationStageResult
        {
            bool applied = false;
            std::vector<std::wstring> warnings;
        };

        MigrationStageResult migrateConfigStage(const std::filesystem::path& configPath,
            PreferencesStore& store,
            const std::wstring& guiDomain,
            const std::wstring& backupDomain,
            bool skipExisting)
        {
            MigrationStageResult result;
            if (!std::filesystem::exists(configPath))
            {
                result.warnings.emplace_back(L"Plik config.xml nie został odnaleziony: " + configPath.wstring());
                return result;
            }

            auto snapshot = readGuiPreferencesFromConfig(configPath, result.warnings);
            if (!snapshot)
            {
                return result;
            }

            if (skipExisting)
            {
                backfillGuiPreferences(store, guiDomain, backupDomain, *snapshot);
            }
            else
            {
                persistGuiPreferences(store, guiDomain, backupDomain, *snapshot);
            }

            result.applied = true;
            return result;
        }

        MigrationStageResult migrateRegistryStage(const std::filesystem::path& registryPath,
            PreferencesStore& store,
            bool skipExisting)
        {
            MigrationStageResult result;
            if (!std::filesystem::exists(registryPath))
            {
                result.warnings.emplace_back(L"Plik eksportu rejestru nie został odnaleziony: " + registryPath.wstring());
                return result;
            }

            auto content = loadRegistryExport(registryPath, result.warnings);
            if (!content)
            {
                return result;
            }

            std::wistringstream stream(*content);
            std::wstring line;
            std::wstring currentDomain;
            bool domainActive = false;

            while (std::getline(stream, line))
            {
                if (!line.empty() && line.back() == L'\r')
                {
                    line.pop_back();
                }

                if (line.empty())
                {
                    continue;
                }

                if (line.front() == L';')
                {
                    continue;
                }

                if (line.front() == L'[')
                {
                    const auto closing = line.find(L']');
                    if (closing == std::wstring::npos)
                    {
                        continue;
                    }

                    std::wstring path = line.substr(1, closing - 1);
                    if (path == kRegistryRoot)
                    {
                        currentDomain.clear();
                        domainActive = true;
                    }
                    else if (path.size() > kRegistryRoot.size() + 1 && path.rfind(kRegistryRoot, 0) == 0 && path[kRegistryRoot.size()] == L'\\')
                    {
                        currentDomain = path.substr(kRegistryRoot.size() + 1);
                        std::replace(currentDomain.begin(), currentDomain.end(), L'\\', L'/');
                        domainActive = true;
                    }
                    else
                    {
                        domainActive = false;
                    }

                    continue;
                }

                if (!domainActive)
                {
                    continue;
                }

                auto parsed = parseRegistryLine(line);
                if (!parsed)
                {
                    continue;
                }

                const std::wstring domain = currentDomain;
                const std::wstring key = parsed->key;

                switch (parsed->type)
                {
                    case RegistryLine::Type::String:
                    {
                        const std::wstring decoded = decodeQuotedValue(parsed->rawValue);
                        if (!applyString(store, domain, key, decoded, skipExisting))
                        {
                            result.warnings.emplace_back(L"Nie udało się zapisać wartości tekstowej dla " + domain + L"/" + key);
                        }
                        break;
                    }
                    case RegistryLine::Type::DWord:
                    {
                        try
                        {
                            const auto numeric = static_cast<std::uint32_t>(std::stoul(parsed->rawValue, nullptr, 16));
                            if (!applyBoolean(store, domain, key, numeric, skipExisting))
                            {
                                result.warnings.emplace_back(L"Nie udało się zapisać wartości logicznej dla " + domain + L"/" + key);
                            }
                        }
                        catch (const std::exception&)
                        {
                            result.warnings.emplace_back(L"Niepoprawny format DWORD w eksporcie rejestru dla " + domain + L"/" + key);
                        }
                        break;
                    }
                    case RegistryLine::Type::QWord:
                    {
                        try
                        {
                            const auto numeric = static_cast<std::uint64_t>(std::stoull(parsed->rawValue, nullptr, 16));
                            if (!applyInt64(store, domain, key, static_cast<std::int64_t>(numeric), skipExisting))
                            {
                                result.warnings.emplace_back(L"Nie udało się zapisać wartości liczbowej dla " + domain + L"/" + key);
                            }
                        }
                        catch (const std::exception&)
                        {
                            result.warnings.emplace_back(L"Niepoprawny format QWORD w eksporcie rejestru dla " + domain + L"/" + key);
                        }
                        break;
                    }
                    case RegistryLine::Type::Delete:
                    {
                        store.remove(domain, key);
                        break;
                    }
                    case RegistryLine::Type::Unsupported:
                    default:
                        result.warnings.emplace_back(L"Pominięto nieobsługiwany wpis rejestru dla " + domain + L"/" + key);
                        break;
                }
            }

            result.applied = true;
            return result;
        }
    }

    PreferencesMigrationReport migratePreferences(const PreferencesMigrationConfig& config,
        PreferencesStore& store)
    {
        PreferencesMigrationReport report;

        if (config.configXmlPath)
        {
            auto stage = migrateConfigStage(*config.configXmlPath, store, config.guiDomain, config.backupDomain, config.skipExistingValues);
            report.configXmlImported = stage.applied;
            report.warnings.insert(report.warnings.end(), stage.warnings.begin(), stage.warnings.end());
        }

        if (config.registryExportPath)
        {
            auto stage = migrateRegistryStage(*config.registryExportPath, store, config.skipExistingValues);
            report.registryExportImported = stage.applied;
            report.warnings.insert(report.warnings.end(), stage.warnings.begin(), stage.warnings.end());
        }

        return report;
    }
}
