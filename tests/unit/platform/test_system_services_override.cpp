#include "catch.hpp"

#include "Platform/SystemServices.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

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

    class FakeNotificationService final : public npp::platform::NotificationService
    {
    public:
        bool post(const npp::platform::NotificationRequest& request) override
        {
            delivered.push_back(request);
            return true;
        }

        bool withdraw(const std::wstring& identifier) override
        {
            const auto before = delivered.size();
            delivered.erase(std::remove_if(delivered.begin(), delivered.end(), [&](const auto& entry) {
                return entry.identifier == identifier;
            }), delivered.end());
            return delivered.size() != before;
        }

        std::vector<npp::platform::NotificationRequest> delivered;
    };

    class FakeStatusItem final : public npp::platform::StatusItem
    {
    public:
        explicit FakeStatusItem(npp::platform::StatusItemDescriptor descriptor)
            : descriptor_(std::move(descriptor))
        {
        }

        bool show() override
        {
            ++showCount;
            visible = true;
            return true;
        }

        bool hide() override
        {
            ++hideCount;
            const bool wasVisible = visible;
            visible = false;
            return wasVisible;
        }

        bool isVisible() const override
        {
            return visible;
        }

        bool reinstall() override
        {
            ++reinstallCount;
            return visible;
        }

        npp::platform::StatusItemDescriptor descriptor_;
        std::size_t showCount = 0;
        std::size_t hideCount = 0;
        std::size_t reinstallCount = 0;
        bool visible = false;
    };

    class FakeStatusItemService final : public npp::platform::StatusItemService
    {
    public:
        std::unique_ptr<npp::platform::StatusItem> create(const npp::platform::StatusItemDescriptor& descriptor) override
        {
            auto item = std::make_unique<FakeStatusItem>(descriptor);
            created.push_back(item.get());
            return item;
        }

        std::vector<FakeStatusItem*> created;
    };

    class FakePrintService final : public npp::platform::PrintService
    {
    public:
        bool printDocument(const npp::platform::PrintDocumentRequest& request) override
        {
            requests.push_back(request);
            return allowSuccess;
        }

        bool allowSuccess = true;
        std::vector<npp::platform::PrintDocumentRequest> requests;
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

        npp::platform::DocumentOpenQueue& documentOpenQueue() override
        {
            return documentQueue_;
        }

        npp::platform::SharingCommandQueue& sharingCommands() override
        {
            return sharingQueue_;
        }

        npp::platform::NotificationService& notifications() override
        {
            return notifications_;
        }

        npp::platform::StatusItemService& statusItems() override
        {
            return statusItems_;
        }

        npp::platform::PrintService& printing() override
        {
            return printService_;
        }

        std::function<std::unique_ptr<npp::platform::FileWatcher>()> fileWatcherFactory;

    private:
        FakeClipboard clipboard_;
        FakePreferences preferences_;
        npp::platform::DocumentOpenQueue documentQueue_;
        npp::platform::SharingCommandQueue sharingQueue_;
        FakeNotificationService notifications_;
        FakeStatusItemService statusItems_;
        FakePrintService printService_;
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

        auto& openQueue = services.documentOpenQueue();
        npp::platform::DocumentOpenRequest request{};
        request.paths = {observedPath / "queued.txt"};
        request.source = npp::platform::DocumentOpenSource::DragAndDrop;
        openQueue.enqueue(request);
        const auto queued = openQueue.poll();
        REQUIRE(queued.has_value());
        CHECK(queued->paths.size() == 1);
        CHECK(queued->paths.front() == observedPath / "queued.txt");
        CHECK(queued->source == npp::platform::DocumentOpenSource::DragAndDrop);

        auto& sharingQueue = services.sharingCommands();
        npp::platform::SharingCommand share{};
        share.type = npp::platform::SharingCommandType::QuickLookPreview;
        share.items = {observedPath / "preview.txt"};
        sharingQueue.enqueue(share);

        npp::platform::SharingCommand serviceAction{};
        serviceAction.type = npp::platform::SharingCommandType::ServicesMenu;
        serviceAction.items = {observedPath / "service.rtf"};
        serviceAction.serviceIdentifier = L"com.example.NotepadPlusPlus.test"s;
        sharingQueue.enqueue(serviceAction);

        const auto firstShare = sharingQueue.poll();
        REQUIRE(firstShare.has_value());
        CHECK(firstShare->type == npp::platform::SharingCommandType::QuickLookPreview);
        REQUIRE(firstShare->items.size() == 1);
        CHECK(firstShare->items.front() == observedPath / "preview.txt");
        CHECK_FALSE(firstShare->serviceIdentifier.has_value());

        const auto secondShare = sharingQueue.poll();
        REQUIRE(secondShare.has_value());
        CHECK(secondShare->type == npp::platform::SharingCommandType::ServicesMenu);
        REQUIRE(secondShare->items.size() == 1);
        CHECK(secondShare->items.front() == observedPath / "service.rtf");
        REQUIRE(secondShare->serviceIdentifier.has_value());
        CHECK(secondShare->serviceIdentifier == L"com.example.NotepadPlusPlus.test"s);

        auto& notificationService = services.notifications();
        npp::platform::NotificationRequest info{};
        info.identifier = L"tests.notification.success"s;
        info.title = L"Background Save"s;
        info.body = L"Document saved successfully."s;
        info.urgency = npp::platform::NotificationUrgency::Information;
        info.playSound = true;
        info.dismissAfter = std::chrono::milliseconds{1500};

        REQUIRE(notificationService.post(info));
        auto* fakeNotifications = dynamic_cast<FakeNotificationService*>(&notificationService);
        REQUIRE(fakeNotifications);
        REQUIRE(fakeNotifications->delivered.size() == 1);
        CHECK(fakeNotifications->delivered.front().identifier == info.identifier);
        CHECK(fakeNotifications->delivered.front().playSound);
        CHECK(fakeNotifications->withdraw(info.identifier));
        CHECK(fakeNotifications->delivered.empty());

        auto& statusService = services.statusItems();
        auto* fakeStatusService = dynamic_cast<FakeStatusItemService*>(&statusService);
        REQUIRE(fakeStatusService);

        npp::platform::StatusItemDescriptor statusDescriptor{};
        statusDescriptor.identifier = L"tests.status.item"s;
        statusDescriptor.tooltip = L"Status item"s;
#ifdef _WIN32
        statusDescriptor.windows.owner = reinterpret_cast<void*>(0x1234);
        statusDescriptor.windows.iconId = 512u;
        statusDescriptor.windows.callbackMessage = 0x400u;
        statusDescriptor.windows.icon = reinterpret_cast<void*>(0x5678);
#endif

        auto statusItem = statusService.create(statusDescriptor);
        REQUIRE(statusItem);
        CHECK_FALSE(statusItem->isVisible());
        CHECK(statusItem->show());
        CHECK(statusItem->isVisible());
        CHECK(statusItem->reinstall());
        CHECK(statusItem->hide());
        CHECK_FALSE(statusItem->isVisible());
        REQUIRE_FALSE(fakeStatusService->created.empty());
        auto* trackedStatus = fakeStatusService->created.back();
        REQUIRE(trackedStatus);
        CHECK(trackedStatus->showCount == 1);
        CHECK(trackedStatus->hideCount == 1);
        CHECK(trackedStatus->reinstallCount == 1);

        auto& printService = services.printing();
        auto* fakePrint = dynamic_cast<FakePrintService*>(&printService);
        REQUIRE(fakePrint);

        npp::platform::PrintDocumentRequest printRequest{};
        printRequest.jobTitle = L"UnitTest.txt"s;
        printRequest.showPrintDialog = false;
        printRequest.selectionStart = 0u;
        printRequest.selectionEnd = 0u;
        printRequest.printSelectionOnly = false;

        CHECK(fakePrint->requests.empty());
        CHECK(printService.printDocument(printRequest));
        REQUIRE(fakePrint->requests.size() == 1);
        CHECK(fakePrint->requests.front().jobTitle == L"UnitTest.txt"s);
        CHECK_FALSE(fakePrint->requests.front().printSelectionOnly);
    }

    auto& fallbackServices = npp::platform::SystemServices::instance();
    auto fallbackWatcher = fallbackServices.createFileWatcher();
    REQUIRE(fallbackWatcher);
#if defined(__APPLE__)
    CHECK(fallbackWatcher->watch(std::filesystem::path{"/tmp"}, FileWatchEvent::FileName));
#else
    CHECK_FALSE(fallbackWatcher->watch(std::filesystem::path{"/tmp"}, FileWatchEvent::FileName));
#endif
    CHECK(fallbackServices.documentOpenQueue().empty());
    CHECK(fallbackServices.sharingCommands().empty());
    CHECK_FALSE(fallbackServices.notifications().withdraw(L"unknown.identifier"));

    npp::platform::StatusItemDescriptor fallbackDescriptor{};
    fallbackDescriptor.identifier = L"fallback.status"s;
    fallbackDescriptor.tooltip = L"Fallback"s;
#ifdef _WIN32
    fallbackDescriptor.windows.owner = nullptr;
    fallbackDescriptor.windows.iconId = 0u;
    fallbackDescriptor.windows.callbackMessage = 0u;
    fallbackDescriptor.windows.icon = nullptr;
#endif
    auto fallbackStatus = fallbackServices.statusItems().create(fallbackDescriptor);
    REQUIRE(fallbackStatus);
    CHECK(fallbackStatus->show());
    CHECK(fallbackStatus->isVisible());
    CHECK_FALSE(fallbackStatus->reinstall());
    CHECK(fallbackStatus->hide());
    CHECK_FALSE(fallbackStatus->isVisible());

    npp::platform::PrintDocumentRequest unsupportedPrint{};
    CHECK_FALSE(fallbackServices.printing().printDocument(unsupportedPrint));
}
