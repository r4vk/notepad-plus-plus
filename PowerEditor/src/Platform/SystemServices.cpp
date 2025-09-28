#include "Platform/SystemServices.h"

#include <memory>
#include <mutex>
#include <optional>

namespace npp::platform
{
    namespace
    {
        class StubClipboardService final : public ClipboardService
        {
        public:
            bool setText(const std::wstring& text) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                text_ = text;
                hasValue_ = true;
                return true;
            }

            std::optional<std::wstring> getText() override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!hasValue_)
                {
                    return std::nullopt;
                }

                return text_;
            }

        private:
            std::mutex mutex_;
            std::wstring text_;
            bool hasValue_ = false;
        };

        class StubFileWatcher final : public FileWatcher
        {
        public:
            bool watch(const std::filesystem::path&, FileWatchEvent, const FileWatchOptions&) override
            {
                return false;
            }

            std::optional<FileChangeNotification> poll() override
            {
                return std::nullopt;
            }
        };

        class StubSystemServices final : public SystemServices
        {
        public:
            ClipboardService& clipboard() override
            {
                return clipboard_;
            }

            std::unique_ptr<FileWatcher> createFileWatcher() override
            {
                return std::make_unique<StubFileWatcher>();
            }

        private:
            StubClipboardService clipboard_;
        };

#ifdef _WIN32
        std::unique_ptr<SystemServices> createWindowsSystemServices();
#endif

        std::unique_ptr<SystemServices> createPlatformServices()
        {
#ifdef _WIN32
            return createWindowsSystemServices();
#else
            return std::make_unique<StubSystemServices>();
#endif
        }
    }

    SystemServices& SystemServices::instance()
    {
        static std::once_flag once;
        static std::unique_ptr<SystemServices> services;

        std::call_once(once, [] { services = createPlatformServices(); });
        return *services;
    }
}

