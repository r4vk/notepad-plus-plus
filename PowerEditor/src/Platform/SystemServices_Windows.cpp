#ifdef _WIN32

#include "Platform/SystemServices.h"

#include "WinControls/ReadDirectoryChanges/ReadDirectoryChanges.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace npp::platform
{
    namespace
    {
        constexpr const wchar_t* kPreferencesRootKey = L"Software\\Notepad++\\macOSPort\\Preferences";

        std::wstring sanitizeDomain(const std::wstring& domain)
        {
            std::wstring sanitized = domain;
            std::replace(sanitized.begin(), sanitized.end(), L'/', L'\\');
            return sanitized;
        }

        std::wstring buildSubKey(const std::wstring& domain)
        {
            if (domain.empty())
            {
                return kPreferencesRootKey;
            }

            std::wstring subKey = kPreferencesRootKey;
            subKey += L"\\";
            subKey += sanitizeDomain(domain);
            return subKey;
        }

        class WindowsPreferencesStore final : public PreferencesStore
        {
        public:
            bool setString(const std::wstring& domain, const std::wstring& key, const std::wstring& value) override
            {
                const DWORD size = static_cast<DWORD>((value.size() + 1u) * sizeof(wchar_t));
                return setValue(domain, key, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), size);
            }

            std::optional<std::wstring> getString(const std::wstring& domain, const std::wstring& key) override
            {
                std::vector<wchar_t> buffer(256u);
                DWORD size = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
                const std::wstring subKey = buildSubKey(domain);

                LONG status = ::RegGetValueW(HKEY_CURRENT_USER, subKey.c_str(), key.c_str(), RRF_RT_REG_SZ, nullptr, buffer.data(), &size);
                if (status == ERROR_MORE_DATA)
                {
                    buffer.resize(size / sizeof(wchar_t));
                    status = ::RegGetValueW(HKEY_CURRENT_USER, subKey.c_str(), key.c_str(), RRF_RT_REG_SZ, nullptr, buffer.data(), &size);
                }

                if (status != ERROR_SUCCESS)
                {
                    return std::nullopt;
                }

                buffer.resize(size / sizeof(wchar_t));

                if (!buffer.empty() && buffer.back() == L'\0')
                {
                    buffer.pop_back();
                }

                return std::wstring(buffer.begin(), buffer.end());
            }

            bool setInt64(const std::wstring& domain, const std::wstring& key, std::int64_t value) override
            {
                const std::int64_t data = value;
                return setValue(domain, key, REG_QWORD, reinterpret_cast<const BYTE*>(&data), sizeof(data));
            }

            std::optional<std::int64_t> getInt64(const std::wstring& domain, const std::wstring& key) override
            {
                const std::wstring subKey = buildSubKey(domain);
                std::int64_t value = 0;
                DWORD size = sizeof(value);
                if (::RegGetValueW(HKEY_CURRENT_USER, subKey.c_str(), key.c_str(), RRF_RT_REG_QWORD, nullptr, &value, &size) != ERROR_SUCCESS)
                {
                    return std::nullopt;
                }

                return value;
            }

            bool setBoolean(const std::wstring& domain, const std::wstring& key, bool value) override
            {
                const DWORD data = value ? 1u : 0u;
                return setValue(domain, key, REG_DWORD, reinterpret_cast<const BYTE*>(&data), sizeof(data));
            }

            std::optional<bool> getBoolean(const std::wstring& domain, const std::wstring& key) override
            {
                const std::wstring subKey = buildSubKey(domain);
                DWORD data = 0u;
                DWORD size = sizeof(data);
                if (::RegGetValueW(HKEY_CURRENT_USER, subKey.c_str(), key.c_str(), RRF_RT_REG_DWORD, nullptr, &data, &size) != ERROR_SUCCESS)
                {
                    return std::nullopt;
                }

                return data != 0u;
            }

            bool remove(const std::wstring& domain, const std::wstring& key) override
            {
                const std::wstring subKey = buildSubKey(domain);
                const LONG status = ::RegDeleteKeyValueW(HKEY_CURRENT_USER, subKey.c_str(), key.c_str());
                if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
                {
                    return false;
                }

                return status == ERROR_SUCCESS;
            }

            bool clearDomain(const std::wstring& domain) override
            {
                const std::wstring subKey = buildSubKey(domain);
                const LONG status = ::RegDeleteTreeW(HKEY_CURRENT_USER, subKey.c_str());
                if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
                {
                    return false;
                }

                return status == ERROR_SUCCESS;
            }

        private:
            bool setValue(const std::wstring& domain, const std::wstring& key, DWORD type, const BYTE* data, DWORD size)
            {
                const std::wstring subKey = buildSubKey(domain);
                HKEY handle = nullptr;
                const LONG createStatus = ::RegCreateKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &handle, nullptr);
                if (createStatus != ERROR_SUCCESS)
                {
                    return false;
                }

                const LONG status = ::RegSetValueExW(handle, key.c_str(), 0, type, data, size);
                ::RegCloseKey(handle);
                return status == ERROR_SUCCESS;
            }
        };

        class ClipboardCloser
        {
        public:
            void activate()
            {
                active_ = true;
            }

            ~ClipboardCloser()
            {
                if (active_)
                {
                    ::CloseClipboard();
                }
            }

        private:
            bool active_ = false;
        };

        class WindowsClipboardService final : public ClipboardService
        {
        public:
            bool setText(const std::wstring& text) override
            {
                ClipboardCloser closer;

                if (!::OpenClipboard(nullptr))
                {
                    return false;
                }

                closer.activate();

                if (!::EmptyClipboard())
                {
                    return false;
                }

                const SIZE_T bytes = (text.size() + 1u) * sizeof(wchar_t);
                HGLOBAL handle = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
                if (!handle)
                {
                    return false;
                }

                void* memory = ::GlobalLock(handle);
                if (!memory)
                {
                    ::GlobalFree(handle);
                    return false;
                }

                std::copy_n(text.c_str(), text.size() + 1u, static_cast<wchar_t*>(memory));
                ::GlobalUnlock(handle);

                if (!::SetClipboardData(CF_UNICODETEXT, handle))
                {
                    ::GlobalFree(handle);
                    return false;
                }

                return true;
            }

            std::optional<std::wstring> getText() override
            {
                ClipboardCloser closer;

                if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
                {
                    return std::nullopt;
                }

                if (!::OpenClipboard(nullptr))
                {
                    return std::nullopt;
                }

                closer.activate();

                const HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);
                if (!handle)
                {
                    return std::nullopt;
                }

                const wchar_t* data = static_cast<const wchar_t*>(::GlobalLock(handle));
                if (!data)
                {
                    return std::nullopt;
                }

                std::wstring text(data);
                ::GlobalUnlock(handle);
                return text;
            }
        };

        DWORD toNativeMask(FileWatchEvent events)
        {
            DWORD mask = 0;
            if (hasFlag(events, FileWatchEvent::FileName))
            {
                mask |= FILE_NOTIFY_CHANGE_FILE_NAME;
            }
            if (hasFlag(events, FileWatchEvent::DirectoryName))
            {
                mask |= FILE_NOTIFY_CHANGE_DIR_NAME;
            }
            if (hasFlag(events, FileWatchEvent::Attributes))
            {
                mask |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
            }
            if (hasFlag(events, FileWatchEvent::Size))
            {
                mask |= FILE_NOTIFY_CHANGE_SIZE;
            }
            if (hasFlag(events, FileWatchEvent::LastWrite))
            {
                mask |= FILE_NOTIFY_CHANGE_LAST_WRITE;
            }
            if (hasFlag(events, FileWatchEvent::LastAccess))
            {
                mask |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
            }
            if (hasFlag(events, FileWatchEvent::Creation))
            {
                mask |= FILE_NOTIFY_CHANGE_CREATION;
            }
            if (hasFlag(events, FileWatchEvent::Security))
            {
                mask |= FILE_NOTIFY_CHANGE_SECURITY;
            }

            return mask;
        }

        FileChangeType toChangeType(DWORD action)
        {
            switch (action)
            {
                case FILE_ACTION_ADDED:
                    return FileChangeType::Added;
                case FILE_ACTION_REMOVED:
                    return FileChangeType::Removed;
                case FILE_ACTION_MODIFIED:
                    return FileChangeType::Modified;
                case FILE_ACTION_RENAMED_OLD_NAME:
                    return FileChangeType::RenamedOldName;
                case FILE_ACTION_RENAMED_NEW_NAME:
                    return FileChangeType::RenamedNewName;
                default:
                    return FileChangeType::Modified;
            }
        }

        class WindowsFileWatcher final : public FileWatcher
        {
        public:
            WindowsFileWatcher() = default;

            ~WindowsFileWatcher() override
            {
                stop();
            }

            void start() override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!running_)
                {
                    changes_.Init();
                    running_ = true;
                }
            }

            void stop() noexcept override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (running_)
                {
                    changes_.Terminate();
                    running_ = false;
                }
            }

            bool watch(const std::filesystem::path& directory, FileWatchEvent events, const FileWatchOptions& options) override
            {
                if (directory.empty())
                {
                    return false;
                }

                start();

                const std::wstring widePath = directory.wstring();
                changes_.AddDirectory(widePath.c_str(), options.watchSubdirectories ? TRUE : FALSE, toNativeMask(events), options.bufferSize);
                return true;
            }

            std::optional<FileChangeNotification> poll() override
            {
                DWORD action = 0;
                std::wstring path;
                if (!changes_.Pop(action, path))
                {
                    return std::nullopt;
                }

                FileChangeNotification notification{};
                notification.type = toChangeType(action);
                notification.path = std::filesystem::path(path);
                return notification;
            }

        private:
            std::mutex mutex_;
            bool running_ = false;
            CReadDirectoryChanges changes_;
        };

        class WindowsSystemServices final : public SystemServices
        {
        public:
            ClipboardService& clipboard() override
            {
                return clipboard_;
            }

            std::unique_ptr<FileWatcher> createFileWatcher() override
            {
                return std::make_unique<WindowsFileWatcher>();
            }

            PreferencesStore& preferences() override
            {
                return preferences_;
            }

        private:
            WindowsClipboardService clipboard_;
            WindowsPreferencesStore preferences_;
        };
    }

    std::unique_ptr<SystemServices> createWindowsSystemServices()
    {
        return std::make_unique<WindowsSystemServices>();
    }
}

#endif

