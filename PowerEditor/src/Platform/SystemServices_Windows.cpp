#ifdef _WIN32

#include "Platform/SystemServices.h"

#include "WinControls/ReadDirectoryChanges/ReadDirectoryChanges.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <optional>

namespace npp::platform
{
    namespace
    {
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

        private:
            WindowsClipboardService clipboard_;
        };
    }

    std::unique_ptr<SystemServices> createWindowsSystemServices()
    {
        return std::make_unique<WindowsSystemServices>();
    }
}

#endif

