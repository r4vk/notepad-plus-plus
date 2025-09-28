#include "Platform/SystemServices.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <variant>

namespace npp::platform
{
    namespace
    {
        class StubPreferencesStore final : public PreferencesStore
        {
        public:
            bool setString(const std::wstring& domain, const std::wstring& key, const std::wstring& value) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                values_[domain][key] = value;
                return true;
            }

            std::optional<std::wstring> getString(const std::wstring& domain, const std::wstring& key) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto domainIt = values_.find(domain);
                if (domainIt == values_.end())
                {
                    return std::nullopt;
                }

                auto keyIt = domainIt->second.find(key);
                if (keyIt == domainIt->second.end() || !std::holds_alternative<std::wstring>(keyIt->second))
                {
                    return std::nullopt;
                }

                return std::get<std::wstring>(keyIt->second);
            }

            bool setInt64(const std::wstring& domain, const std::wstring& key, std::int64_t value) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                values_[domain][key] = value;
                return true;
            }

            std::optional<std::int64_t> getInt64(const std::wstring& domain, const std::wstring& key) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto domainIt = values_.find(domain);
                if (domainIt == values_.end())
                {
                    return std::nullopt;
                }

                auto keyIt = domainIt->second.find(key);
                if (keyIt == domainIt->second.end() || !std::holds_alternative<std::int64_t>(keyIt->second))
                {
                    return std::nullopt;
                }

                return std::get<std::int64_t>(keyIt->second);
            }

            bool setBoolean(const std::wstring& domain, const std::wstring& key, bool value) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                values_[domain][key] = value;
                return true;
            }

            std::optional<bool> getBoolean(const std::wstring& domain, const std::wstring& key) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto domainIt = values_.find(domain);
                if (domainIt == values_.end())
                {
                    return std::nullopt;
                }

                auto keyIt = domainIt->second.find(key);
                if (keyIt == domainIt->second.end() || !std::holds_alternative<bool>(keyIt->second))
                {
                    return std::nullopt;
                }

                return std::get<bool>(keyIt->second);
            }

            bool remove(const std::wstring& domain, const std::wstring& key) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto domainIt = values_.find(domain);
                if (domainIt == values_.end())
                {
                    return false;
                }

                const auto erased = domainIt->second.erase(key) > 0u;
                if (domainIt->second.empty())
                {
                    values_.erase(domainIt);
                }

                return erased;
            }

            bool clearDomain(const std::wstring& domain) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (domain.empty())
                {
                    const bool hadValues = !values_.empty();
                    values_.clear();
                    return hadValues;
                }

                return values_.erase(domain) > 0u;
            }

        private:
            using Value = std::variant<std::wstring, std::int64_t, bool>;
            using Domain = std::map<std::wstring, Value>;

            std::map<std::wstring, Domain> values_;
            std::mutex mutex_;
        };

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

            PreferencesStore& preferences() override
            {
                return preferences_;
            }

        private:
            StubClipboardService clipboard_;
            StubPreferencesStore preferences_;
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

