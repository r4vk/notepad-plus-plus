#include "catch.hpp"

#include "Platform/SystemServices.h"

#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

using namespace std::string_literals;

namespace
{
    class FakeClipboard final : public npp::platform::ClipboardService
    {
    public:
        bool setText(const std::wstring& text) override
        {
            text_ = text;
            return true;
        }

        std::optional<std::wstring> getText() override
        {
            if (text_.empty())
            {
                return std::nullopt;
            }

            return text_;
        }

    private:
        std::wstring text_;
    };

    class FakeFileWatcher final : public npp::platform::FileWatcher
    {
    public:
        bool watch(const std::filesystem::path& directory, npp::platform::FileWatchEvent events, const npp::platform::FileWatchOptions&) override
        {
            watchedDirectory = directory;
            mask = events;
            started = true;
            return true;
        }

        std::optional<npp::platform::FileChangeNotification> poll() override
        {
            if (notifications.empty())
            {
                return std::nullopt;
            }

            auto front = notifications.front();
            notifications.pop_front();
            return front;
        }

        void push(npp::platform::FileChangeNotification notification)
        {
            notifications.push_back(std::move(notification));
        }

        std::filesystem::path watchedDirectory;
        npp::platform::FileWatchEvent mask = npp::platform::FileWatchEvent::None;
        bool started = false;

    private:
        std::deque<npp::platform::FileChangeNotification> notifications;
    };

    class FakePreferences final : public npp::platform::PreferencesStore
    {
    public:
        bool setString(const std::wstring& domain, const std::wstring& key, const std::wstring& value) override
        {
            strings[domain + L"/" + key] = value;
            return true;
        }

        std::optional<std::wstring> getString(const std::wstring& domain, const std::wstring& key) override
        {
            const auto it = strings.find(domain + L"/" + key);
            if (it == strings.end())
            {
                return std::nullopt;
            }

            return it->second;
        }

        bool setInt64(const std::wstring& domain, const std::wstring& key, std::int64_t value) override
        {
            integers[domain + L"/" + key] = value;
            return true;
        }

        std::optional<std::int64_t> getInt64(const std::wstring& domain, const std::wstring& key) override
        {
            const auto it = integers.find(domain + L"/" + key);
            if (it == integers.end())
            {
                return std::nullopt;
            }

            return it->second;
        }

        bool setBoolean(const std::wstring& domain, const std::wstring& key, bool value) override
        {
            booleans[domain + L"/" + key] = value;
            return true;
        }

        std::optional<bool> getBoolean(const std::wstring& domain, const std::wstring& key) override
        {
            const auto it = booleans.find(domain + L"/" + key);
            if (it == booleans.end())
            {
                return std::nullopt;
            }

            return it->second;
        }

        bool remove(const std::wstring& domain, const std::wstring& key) override
        {
            const auto composed = domain + L"/" + key;
            const auto erasedString = strings.erase(composed) > 0u;
            const auto erasedInt = integers.erase(composed) > 0u;
            const auto erasedBool = booleans.erase(composed) > 0u;
            return erasedString || erasedInt || erasedBool;
        }

        bool clearDomain(const std::wstring& domain) override
        {
            bool cleared = false;
            for (auto it = strings.begin(); it != strings.end();)
            {
                if (it->first.rfind(domain + L"/", 0) == 0)
                {
                    it = strings.erase(it);
                    cleared = true;
                }
                else
                {
                    ++it;
                }
            }

            for (auto it = integers.begin(); it != integers.end();)
            {
                if (it->first.rfind(domain + L"/", 0) == 0)
                {
                    it = integers.erase(it);
                    cleared = true;
                }
                else
                {
                    ++it;
                }
            }

            for (auto it = booleans.begin(); it != booleans.end();)
            {
                if (it->first.rfind(domain + L"/", 0) == 0)
                {
                    it = booleans.erase(it);
                    cleared = true;
                }
                else
                {
                    ++it;
                }
            }

            return cleared;
        }

    private:
        std::map<std::wstring, std::wstring> strings;
        std::map<std::wstring, std::int64_t> integers;
        std::map<std::wstring, bool> booleans;
    };

    class FakeSystemServices final : public npp::platform::SystemServices
    {
    public:
        npp::platform::ClipboardService& clipboard() override
        {
            return clipboard_;
        }

        std::unique_ptr<npp::platform::FileWatcher> createFileWatcher() override
        {
            if (fileWatcherFactory)
            {
                return fileWatcherFactory();
            }

            return std::make_unique<FakeFileWatcher>();
        }

        npp::platform::PreferencesStore& preferences() override
        {
            return preferences_;
        }

        std::function<std::unique_ptr<npp::platform::FileWatcher>()> fileWatcherFactory;

    private:
        FakeClipboard clipboard_;
        FakePreferences preferences_;
    };
}

TEST_CASE("system services override injects clipboard and file watcher mocks", "[platform][system_services]")
{
    using npp::platform::FileWatchEvent;
    using npp::platform::FileChangeType;
    using npp::platform::FileChangeNotification;
    using npp::platform::testing::ScopedSystemServicesOverride;

    std::filesystem::path observedPath;
    FileWatchEvent observedMask = FileWatchEvent::None;
    FakeFileWatcher* watcherHandle = nullptr;

    auto customServices = std::make_unique<FakeSystemServices>();
    customServices->fileWatcherFactory = [&]() {
        auto watcher = std::make_unique<FakeFileWatcher>();
        watcherHandle = watcher.get();
        return watcher;
    };

    ScopedSystemServicesOverride overrideGuard(std::move(customServices));

    {
        auto& services = npp::platform::SystemServices::instance();
        auto watcher = services.createFileWatcher();
        REQUIRE(watcher);

        observedPath = std::filesystem::path{"/tmp/mock-session"};
        observedMask = FileWatchEvent::FileName | FileWatchEvent::LastWrite;

        REQUIRE(watcher->watch(observedPath, observedMask));
        REQUIRE(watcherHandle);
        CHECK(watcherHandle->started);
        CHECK(watcherHandle->watchedDirectory == observedPath);
        CHECK(npp::platform::hasFlag(watcherHandle->mask, FileWatchEvent::FileName));
        CHECK(npp::platform::hasFlag(watcherHandle->mask, FileWatchEvent::LastWrite));

        watcherHandle->push(FileChangeNotification{FileChangeType::Added, observedPath / "document.txt"});

        const auto notification = watcher->poll();
        REQUIRE(notification.has_value());
        CHECK(notification->type == FileChangeType::Added);
        CHECK(notification->path == observedPath / "document.txt");

        auto& clipboard = services.clipboard();
        REQUIRE(clipboard.setText(L"override clipboard"));
        const auto text = clipboard.getText();
        REQUIRE(text.has_value());
        CHECK(*text == L"override clipboard"s);

        auto& preferences = services.preferences();
        REQUIRE(preferences.setBoolean(L"tests", L"remember", true));
        const auto remembered = preferences.getBoolean(L"tests", L"remember");
        REQUIRE(remembered.has_value());
        CHECK(*remembered);
    }

    auto& fallbackServices = npp::platform::SystemServices::instance();
    auto fallbackWatcher = fallbackServices.createFileWatcher();
    REQUIRE(fallbackWatcher);
#if defined(__APPLE__)
    CHECK(fallbackWatcher->watch(std::filesystem::path{"/tmp"}, FileWatchEvent::FileName));
#else
    CHECK_FALSE(fallbackWatcher->watch(std::filesystem::path{"/tmp"}, FileWatchEvent::FileName));
#endif
}
