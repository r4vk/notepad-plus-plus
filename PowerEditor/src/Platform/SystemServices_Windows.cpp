#ifdef _WIN32

#include "Platform/SystemServices.h"

#include "WinControls/ReadDirectoryChanges/ReadDirectoryChanges.h"
#include "WinControls/TrayIcon/trayIconControler.h"
#include "ScintillaComponent/Printer.h"

#include <windows.h>
#include <shellapi.h>

#include <cwchar>
#include <cstddef>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <utility>

#include "resource.h"

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

        class WindowsNotificationService final : public NotificationService
        {
        public:
            WindowsNotificationService()
            {
                registerWindowClass();
                createHostWindow();
                initializeIconData();
            }

            ~WindowsNotificationService() override
            {
                if (iconActive_)
                {
                    ::Shell_NotifyIconW(NIM_DELETE, &iconData_);
                }

                if (window_)
                {
                    ::DestroyWindow(window_);
                }
            }

            bool post(const NotificationRequest& request) override
            {
                if (!ensureIcon())
                {
                    return false;
                }

                configureBalloon(request);
                return ::Shell_NotifyIconW(NIM_MODIFY, &iconData_) == TRUE;
            }

            bool withdraw(const std::wstring& identifier) override
            {
                (void)identifier;
                return false;
            }

        private:
            static constexpr const wchar_t* kWindowClassName = L"NotepadPlusPlus.NotificationHost";

            static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
            {
                if (msg == WM_DESTROY)
                {
                    return 0;
                }

                return ::DefWindowProcW(hwnd, msg, wParam, lParam);
            }

            void registerWindowClass()
            {
                WNDCLASSEXW cls{};
                cls.cbSize = sizeof(WNDCLASSEXW);
                cls.lpfnWndProc = &HostWndProc;
                cls.hInstance = ::GetModuleHandleW(nullptr);
                cls.lpszClassName = kWindowClassName;
                cls.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
                ::RegisterClassExW(&cls);
            }

            void createHostWindow()
            {
                window_ = ::CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClassName, L"Notepad++ Notifications", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
            }

            void initializeIconData()
            {
                iconData_.cbSize = sizeof(NOTIFYICONDATAW);
                iconData_.hWnd = window_;
                iconData_.uID = 1u;
                iconData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
                iconData_.uCallbackMessage = WM_APP + 98;
                iconData_.hIcon = ::LoadIconW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_M30ICON));
                copyTruncated(iconData_.szTip, ARRAYSIZE(iconData_.szTip), L"Notepad++");
            }

            bool ensureIcon()
            {
                if (!window_)
                {
                    return false;
                }

                if (!iconActive_)
                {
                    iconActive_ = ::Shell_NotifyIconW(NIM_ADD, &iconData_) == TRUE;
                    if (iconActive_)
                    {
                        iconData_.uVersion = NOTIFYICON_VERSION_4;
                        ::Shell_NotifyIconW(NIM_SETVERSION, &iconData_);
                    }
                }

                return iconActive_;
            }

            static void copyTruncated(wchar_t* destination, size_t capacity, const std::wstring& value)
            {
                if (!destination || capacity == 0)
                {
                    return;
                }

                const size_t length = std::min(value.size(), capacity - 1u);
                if (length > 0)
                {
                    std::wmemcpy(destination, value.c_str(), length);
                }

                destination[length] = L'\0';
            }

            void configureBalloon(const NotificationRequest& request)
            {
                iconData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
                copyTruncated(iconData_.szTip, ARRAYSIZE(iconData_.szTip), request.title.empty() ? L"Notepad++" : request.title);
                copyTruncated(iconData_.szInfoTitle, ARRAYSIZE(iconData_.szInfoTitle), request.title.empty() ? L"Notepad++" : request.title);
                copyTruncated(iconData_.szInfo, ARRAYSIZE(iconData_.szInfo), request.body);

                switch (request.urgency)
                {
                    case NotificationUrgency::Warning:
                        iconData_.dwInfoFlags = NIIF_WARNING;
                        break;
                    case NotificationUrgency::Error:
                        iconData_.dwInfoFlags = NIIF_ERROR;
                        break;
                    default:
                        iconData_.dwInfoFlags = NIIF_INFO;
                        break;
                }

                if (request.dismissAfter.has_value())
                {
                    const auto clamped = std::clamp(*request.dismissAfter, std::chrono::milliseconds{1000}, std::chrono::milliseconds{60000});
                    iconData_.uTimeout = static_cast<UINT>(clamped.count());
                }
                else
                {
                    iconData_.uTimeout = 10000u;
                }

                if (!request.playSound)
                {
                    iconData_.dwInfoFlags |= NIIF_NOSOUND;
                }
                else
                {
                    iconData_.dwInfoFlags &= ~NIIF_NOSOUND;
                }
            }

            HWND window_ = nullptr;
            NOTIFYICONDATAW iconData_{};
            bool iconActive_ = false;
        };

        class WindowsStatusItem final : public StatusItem
        {
        public:
            explicit WindowsStatusItem(StatusItemDescriptor descriptor)
                : descriptor_(std::move(descriptor))
            {
            }

            ~WindowsStatusItem() override
            {
                hide();
            }

            bool show() override
            {
                if (!ensureController())
                {
                    return false;
                }

                if (controller_->isInTray())
                {
                    return true;
                }

                return controller_->doTrayIcon(NIM_ADD) == 0;
            }

            bool hide() override
            {
                if (!controller_ || !controller_->isInTray())
                {
                    return false;
                }

                return controller_->doTrayIcon(NIM_DELETE) == 0;
            }

            bool isVisible() const override
            {
                return controller_ && controller_->isInTray();
            }

            bool reinstall() override
            {
                if (!controller_ || !controller_->isInTray())
                {
                    return false;
                }

                return controller_->reAddTrayIcon() == 0;
            }

        private:
            bool ensureController()
            {
                if (controller_)
                {
                    return true;
                }

                auto owner = reinterpret_cast<HWND>(descriptor_.windows.owner);
                auto icon = reinterpret_cast<HICON>(descriptor_.windows.icon);
                if (!owner || !icon || descriptor_.windows.callbackMessage == 0u)
                {
                    return false;
                }

                controller_ = std::make_unique<trayIconControler>(owner, static_cast<UINT>(descriptor_.windows.iconId), static_cast<UINT>(descriptor_.windows.callbackMessage), icon, descriptor_.tooltip.c_str());
                return controller_ != nullptr;
            }

            StatusItemDescriptor descriptor_;
            std::unique_ptr<trayIconControler> controller_;
        };

        class WindowsStatusItemService final : public StatusItemService
        {
        public:
            std::unique_ptr<StatusItem> create(const StatusItemDescriptor& descriptor) override
            {
                return std::make_unique<WindowsStatusItem>(descriptor);
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

        class WindowsPrintService final : public PrintService
        {
        public:
            bool printDocument(const PrintDocumentRequest& request) override
            {
                if (!request.windows.instance || !request.windows.owner || !request.windows.editView)
                {
                    return false;
                }

                auto* view = reinterpret_cast<ScintillaEditView*>(request.windows.editView);
                if (!view)
                {
                    return false;
                }

                Printer printer;
                printer.init(static_cast<HINSTANCE>(request.windows.instance),
                             static_cast<HWND>(request.windows.owner),
                             view,
                             request.showPrintDialog,
                             request.selectionStart,
                             request.selectionEnd,
                             request.isRightToLeft);

                const std::size_t printed = printer.doPrint();
                return printed > 0u;
            }
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

            DocumentOpenQueue& documentOpenQueue() override
            {
                return documentQueue_;
            }

            SharingCommandQueue& sharingCommands() override
            {
                return sharingQueue_;
            }

            NotificationService& notifications() override
            {
                return notifications_;
            }

            StatusItemService& statusItems() override
            {
                return statusItems_;
            }

            PrintService& printing() override
            {
                return printing_;
            }

        private:
            WindowsClipboardService clipboard_;
            WindowsPreferencesStore preferences_;
            DocumentOpenQueue documentQueue_;
            SharingCommandQueue sharingQueue_;
            WindowsNotificationService notifications_;
            WindowsStatusItemService statusItems_;
            WindowsPrintService printing_;
        };
    }

    std::unique_ptr<SystemServices> createWindowsSystemServices()
    {
        return std::make_unique<WindowsSystemServices>();
    }
}

#endif

