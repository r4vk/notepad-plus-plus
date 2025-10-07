#include "Platform/SystemServices.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#endif

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

        class StubNotificationService final : public NotificationService
        {
        public:
            bool post(const NotificationRequest& request) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                delivered_.push_back(request);
                return false;
            }

            bool withdraw(const std::wstring& identifier) override
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto before = delivered_.size();
                delivered_.erase(std::remove_if(delivered_.begin(), delivered_.end(), [&](const auto& notification) {
                    return notification.identifier == identifier;
                }), delivered_.end());
                return before != delivered_.size();
            }

        private:
            std::mutex mutex_;
            std::vector<NotificationRequest> delivered_;
        };

        class StubStatusItem final : public StatusItem
        {
        public:
            explicit StubStatusItem(StatusItemDescriptor descriptor)
                : descriptor_(std::move(descriptor))
            {
            }

            bool show() override
            {
                visible_ = true;
                return true;
            }

            bool hide() override
            {
                const bool wasVisible = visible_;
                visible_ = false;
                return wasVisible;
            }

            bool isVisible() const override
            {
                return visible_;
            }

        private:
            StatusItemDescriptor descriptor_;
            bool visible_ = false;
        };

        class StubStatusItemService final : public StatusItemService
        {
        public:
            std::unique_ptr<StatusItem> create(const StatusItemDescriptor& descriptor) override
            {
                return std::make_unique<StubStatusItem>(descriptor);
            }
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

#if defined(__APPLE__)
        namespace
        {
            FileChangeType toChangeType(FSEventStreamEventFlags flags)
            {
                if (flags & kFSEventStreamEventFlagItemRemoved)
                {
                    return FileChangeType::Removed;
                }

                if (flags & kFSEventStreamEventFlagItemRenamed)
                {
                    return FileChangeType::RenamedNewName;
                }

                if (flags & kFSEventStreamEventFlagItemCreated)
                {
                    return FileChangeType::Added;
                }

                return FileChangeType::Modified;
            }

            class MacFileWatcher final : public FileWatcher
            {
            public:
                MacFileWatcher() = default;

                ~MacFileWatcher() override
                {
                    stop();
                }

                void start() override
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    startLocked();
                }

                void stop() noexcept override
                {
                    std::thread worker;

                    {
                        std::lock_guard<std::mutex> lock(stateMutex_);

                        if (runLoop_)
                        {
                            CFRunLoopStop(runLoop_);
                        }

                        worker = std::move(worker_);
                        running_ = false;
                    }

                    if (worker.joinable())
                    {
                        worker.join();
                    }

                    {
                        std::lock_guard<std::mutex> lock(stateMutex_);
                        if (stream_)
                        {
                            FSEventStreamStop(stream_);
                            FSEventStreamInvalidate(stream_);
                            FSEventStreamRelease(stream_);
                            stream_ = nullptr;
                        }

                        runLoop_ = nullptr;
                        directory_.clear();
                        canonicalRoot_.clear();
                        options_ = {};
                        mask_ = FileWatchEvent::None;
                    }

                    {
                        std::lock_guard<std::mutex> lock(queueMutex_);
                        notifications_.clear();
                    }
                }

                bool watch(const std::filesystem::path& directory, FileWatchEvent events, const FileWatchOptions& options = FileWatchOptions{}) override
                {
                    stop();

                    std::error_code ec;
                    if (directory.empty() || !std::filesystem::exists(directory, ec) || !std::filesystem::is_directory(directory, ec))
                    {
                        return false;
                    }

                    std::filesystem::path canonical = std::filesystem::weakly_canonical(directory, ec);
                    if (ec)
                    {
                        canonical = std::filesystem::absolute(directory, ec);
                        if (ec)
                        {
                            canonical = directory;
                        }
                    }

                    CFMutableArrayRef paths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                    if (!paths)
                    {
                        return false;
                    }

                    const std::string pathString = canonical.string();
                    CFStringRef cfPath = CFStringCreateWithCString(kCFAllocatorDefault, pathString.c_str(), kCFStringEncodingUTF8);
                    if (!cfPath)
                    {
                        CFRelease(paths);
                        return false;
                    }

                    CFArrayAppendValue(paths, cfPath);
                    CFRelease(cfPath);

                    FSEventStreamContext context{};
                    context.info = this;

                    const FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer;
                    const CFAbsoluteTime latency = options.watchSubdirectories ? 0.1 : 0.05;

                    FSEventStreamRef stream = FSEventStreamCreate(kCFAllocatorDefault, &MacFileWatcher::streamCallback, &context, paths, kFSEventStreamEventIdSinceNow, latency, flags);

                    CFRelease(paths);

                    if (!stream)
                    {
                        return false;
                    }

                    {
                        std::lock_guard<std::mutex> lock(queueMutex_);
                        notifications_.clear();
                    }

                    {
                        std::lock_guard<std::mutex> lock(stateMutex_);
                        stream_ = stream;
                        directory_ = directory;
                        canonicalRoot_ = std::move(canonical);
                        mask_ = events;
                        options_ = options;
                        running_ = false;
                        runLoop_ = nullptr;
                        startLocked();
                    }

                    return true;
                }

                std::optional<FileChangeNotification> poll() override
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    if (notifications_.empty())
                    {
                        return std::nullopt;
                    }

                    FileChangeNotification notification = std::move(notifications_.front());
                    notifications_.pop_front();
                    return notification;
                }

            private:
                static void streamCallback(ConstFSEventStreamRef, void* clientCallBackInfo, size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId[])
                {
                    auto* self = static_cast<MacFileWatcher*>(clientCallBackInfo);
                    if (self)
                    {
                        self->handleEvents(numEvents, eventPaths, eventFlags);
                    }
                }

                void handleEvents(size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[])
                {
                    auto** paths = static_cast<const char**>(eventPaths);
                    if (!paths)
                    {
                        return;
                    }

                    std::vector<FileChangeNotification> pending;
                    pending.reserve(numEvents);

                    for (size_t i = 0; i < numEvents; ++i)
                    {
                        if (!paths[i])
                        {
                            continue;
                        }

                        std::filesystem::path eventPath{paths[i]};

                        if (!withinScope(eventPath, eventFlags[i]))
                        {
                            continue;
                        }

                        FileChangeNotification notification{};
                        notification.type = toChangeType(eventFlags[i]);
                        notification.path = std::move(eventPath);
                        pending.push_back(std::move(notification));
                    }

                    if (pending.empty())
                    {
                        return;
                    }

                    std::lock_guard<std::mutex> lock(queueMutex_);
                    for (auto& notification : pending)
                    {
                        notifications_.push_back(std::move(notification));
                    }
                }

                bool withinScope(const std::filesystem::path& eventPath, FSEventStreamEventFlags flags) const
                {
                    const bool isDirectory = (flags & kFSEventStreamEventFlagItemIsDir) != 0;

                    if (isDirectory)
                    {
                        if (!hasFlag(mask_, FileWatchEvent::DirectoryName))
                        {
                            return false;
                        }
                    }
                    else if (!(hasFlag(mask_, FileWatchEvent::FileName) || hasFlag(mask_, FileWatchEvent::LastWrite) || hasFlag(mask_, FileWatchEvent::Size) || hasFlag(mask_, FileWatchEvent::Attributes)))
                    {
                        return false;
                    }

                    if (!options_.watchSubdirectories)
                    {
                        std::error_code ec;
                        const auto relative = std::filesystem::relative(eventPath, canonicalRoot_, ec);
                        if (ec)
                        {
                            return false;
                        }

                        if (!relative.empty())
                        {
                            auto it = relative.begin();
                            if (it == relative.end())
                            {
                                return true;
                            }

                            ++it;
                            if (it != relative.end())
                            {
                                return false;
                            }
                        }
                    }

                    return true;
                }

                void startLocked()
                {
                    if (!stream_ || running_)
                    {
                        return;
                    }

                    running_ = true;
                    worker_ = std::thread([this]() { runLoopThread(); });
                }

                void runLoopThread()
                {
                    CFRunLoopRef runLoop = CFRunLoopGetCurrent();

                    {
                        std::lock_guard<std::mutex> lock(stateMutex_);
                        runLoop_ = runLoop;
                    }

                    FSEventStreamScheduleWithRunLoop(stream_, runLoop, kCFRunLoopDefaultMode);

                    if (!FSEventStreamStart(stream_))
                    {
                        std::lock_guard<std::mutex> lock(stateMutex_);
                        FSEventStreamInvalidate(stream_);
                        FSEventStreamRelease(stream_);
                        stream_ = nullptr;
                        runLoop_ = nullptr;
                        running_ = false;
                        return;
                    }

                    CFRunLoopRun();

                    {
                        std::lock_guard<std::mutex> lock(stateMutex_);
                        if (stream_)
                        {
                            FSEventStreamStop(stream_);
                            FSEventStreamInvalidate(stream_);
                            FSEventStreamRelease(stream_);
                            stream_ = nullptr;
                        }

                        runLoop_ = nullptr;
                        running_ = false;
                    }
                }

                std::filesystem::path directory_;
                std::filesystem::path canonicalRoot_;
                FileWatchEvent mask_ = FileWatchEvent::None;
                FileWatchOptions options_{};

                mutable std::mutex queueMutex_;
                std::deque<FileChangeNotification> notifications_;

                mutable std::mutex stateMutex_;
                FSEventStreamRef stream_ = nullptr;
                CFRunLoopRef runLoop_ = nullptr;
                std::thread worker_;
                bool running_ = false;
            };

            std::wstring toWString(CFStringRef string)
            {
                if (!string)
                {
                    return {};
                }

                const CFIndex length = CFStringGetLength(string);
                if (length <= 0)
                {
                    return {};
                }

                std::wstring result(static_cast<std::size_t>(length), L'\0');
                CFStringGetCharacters(string, CFRangeMake(0, length), reinterpret_cast<UniChar*>(result.data()));
                return result;
            }

            id makeNSString(const std::wstring& value)
            {
                if (value.empty())
                {
                    return nullptr;
                }

                CFStringRef cfString = CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(value.data()), static_cast<CFIndex>(value.size()));
                return reinterpret_cast<id>(cfString);
            }

            void setStringProperty(id object, const char* selectorName, const std::wstring& value)
            {
                if (!object || value.empty())
                {
                    return;
                }

                SEL selector = sel_registerName(selectorName);
                id stringValue = makeNSString(value);
                if (!stringValue)
                {
                    return;
                }

                reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(object, selector, stringValue);
                CFRelease(reinterpret_cast<CFTypeRef>(stringValue));
            }

            CFStringRef urgencyString(NotificationUrgency urgency)
            {
                switch (urgency)
                {
                    case NotificationUrgency::Warning:
                        return CFSTR("warning");
                    case NotificationUrgency::Error:
                        return CFSTR("error");
                    default:
                        return CFSTR("info");
                }
            }

            struct NotificationRemovalContext
            {
                id center = nullptr;
                id notification = nullptr;
            };

            void removeDeliveredNotificationCallback(void* context)
            {
                std::unique_ptr<NotificationRemovalContext> removal{static_cast<NotificationRemovalContext*>(context)};
                if (!removal)
                {
                    return;
                }

                if (removal->center && removal->notification)
                {
                    reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(removal->center, sel_registerName("removeDeliveredNotification:"), removal->notification);
                    reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(removal->notification, sel_registerName("release"));
                    reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(removal->center, sel_registerName("release"));
                }
            }

            class MacNotificationService final : public NotificationService
            {
            public:
                bool post(const NotificationRequest& request) override
                {
                    id pool = createAutoreleasePool();
                    if (!pool)
                    {
                        return false;
                    }

                    id notificationClass = objc_getClass("NSUserNotification");
                    id centerClass = objc_getClass("NSUserNotificationCenter");
                    if (!notificationClass || !centerClass)
                    {
                        drainAutoreleasePool(pool);
                        return false;
                    }

                    id notification = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(notificationClass, sel_registerName("alloc"));
                    notification = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(notification, sel_registerName("init"));
                    if (!notification)
                    {
                        drainAutoreleasePool(pool);
                        return false;
                    }

                    setStringProperty(notification, "setTitle:", request.title);
                    setStringProperty(notification, "setSubtitle:", request.subtitle);
                    setStringProperty(notification, "setInformativeText:", request.body);

                    if (!request.identifier.empty())
                    {
                        setStringProperty(notification, "setIdentifier:", request.identifier);
                    }

                    CFMutableDictionaryRef userInfo = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                    if (userInfo)
                    {
                        CFStringRef key = CFSTR("severity");
                        CFDictionarySetValue(userInfo, key, urgencyString(request.urgency));
                        reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(notification, sel_registerName("setUserInfo:"), userInfo);
                        CFRelease(userInfo);
                    }

                    if (request.playSound)
                    {
                        id soundName = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(notificationClass, sel_registerName("defaultSoundName"));
                        reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(notification, sel_registerName("setSoundName:"), soundName);
                    }

                    id center = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(centerClass, sel_registerName("defaultUserNotificationCenter"));
                    if (!center)
                    {
                        reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(notification, sel_registerName("release"));
                        drainAutoreleasePool(pool);
                        return false;
                    }

                    reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(center, sel_registerName("deliverNotification:"), notification);

                    if (request.dismissAfter.has_value())
                    {
                        const auto clamped = std::clamp(*request.dismissAfter, std::chrono::milliseconds{1000}, std::chrono::milliseconds{60000});
                        auto removal = std::make_unique<NotificationRemovalContext>();
                        removal->center = center;
                        removal->notification = notification;
                        reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(removal->center, sel_registerName("retain"));
                        reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(removal->notification, sel_registerName("retain"));
                        dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(clamped.count()) * NSEC_PER_MSEC), dispatch_get_main_queue(), removal.release(), removeDeliveredNotificationCallback);
                    }

                    reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(notification, sel_registerName("release"));
                    drainAutoreleasePool(pool);
                    return true;
                }

                bool withdraw(const std::wstring& identifier) override
                {
                    if (identifier.empty())
                    {
                        return false;
                    }

                    id pool = createAutoreleasePool();
                    if (!pool)
                    {
                        return false;
                    }

                    id centerClass = objc_getClass("NSUserNotificationCenter");
                    if (!centerClass)
                    {
                        drainAutoreleasePool(pool);
                        return false;
                    }

                    id center = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(centerClass, sel_registerName("defaultUserNotificationCenter"));
                    if (!center)
                    {
                        drainAutoreleasePool(pool);
                        return false;
                    }

                    id delivered = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(center, sel_registerName("deliveredNotifications"));
                    if (!delivered)
                    {
                        drainAutoreleasePool(pool);
                        return false;
                    }

                    id enumerator = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(delivered, sel_registerName("objectEnumerator"));
                    if (!enumerator)
                    {
                        drainAutoreleasePool(pool);
                        return false;
                    }

                    bool removed = false;
                    while (true)
                    {
                        id candidate = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(enumerator, sel_registerName("nextObject"));
                        if (!candidate)
                        {
                            break;
                        }

                        id identifierValue = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(candidate, sel_registerName("identifier"));
                        if (!identifierValue)
                        {
                            continue;
                        }

                        if (toWString(reinterpret_cast<CFStringRef>(identifierValue)) == identifier)
                        {
                            reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(center, sel_registerName("removeDeliveredNotification:"), candidate);
                            removed = true;
                            break;
                        }
                    }

                    drainAutoreleasePool(pool);
                    return removed;
                }

            private:
                static id createAutoreleasePool()
                {
                    id poolClass = objc_getClass("NSAutoreleasePool");
                    if (!poolClass)
                    {
                        return nullptr;
                    }

                    id pool = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(poolClass, sel_registerName("alloc"));
                    return reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(pool, sel_registerName("init"));
                }

                static void drainAutoreleasePool(id pool)
                {
                    if (pool)
                    {
                        reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(pool, sel_registerName("drain"));
                    }
                }
            };
        }
#endif

        class StubSystemServices final : public SystemServices
        {
        public:
            ClipboardService& clipboard() override
            {
                return clipboard_;
            }

            std::unique_ptr<FileWatcher> createFileWatcher() override
            {
#if defined(__APPLE__)
                return std::make_unique<MacFileWatcher>();
#else
                return std::make_unique<StubFileWatcher>();
#endif
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

        private:
            StubClipboardService clipboard_;
            StubPreferencesStore preferences_;
            DocumentOpenQueue documentQueue_;
            SharingCommandQueue sharingQueue_;
            StubNotificationService notifications_;
            StubStatusItemService statusItems_;
        };

#ifdef _WIN32
        std::unique_ptr<SystemServices> createWindowsSystemServices();
#elif defined(__APPLE__)
        class MacSystemServices final : public SystemServices
        {
        public:
            ClipboardService& clipboard() override
            {
                return clipboard_;
            }

            std::unique_ptr<FileWatcher> createFileWatcher() override
            {
                return std::make_unique<MacFileWatcher>();
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

        private:
            StubClipboardService clipboard_;
            StubPreferencesStore preferences_;
            DocumentOpenQueue documentQueue_;
            SharingCommandQueue sharingQueue_;
            MacNotificationService notifications_;
            StubStatusItemService statusItems_;
        };
#endif

        std::unique_ptr<SystemServices> createPlatformServices()
        {
#ifdef _WIN32
            return createWindowsSystemServices();
#elif defined(__APPLE__)
            return std::make_unique<MacSystemServices>();
#else
            return std::make_unique<StubSystemServices>();
#endif
        }

        std::mutex instanceMutex;
        std::unique_ptr<SystemServices> defaultServices;
        std::vector<std::unique_ptr<SystemServices>> overrides;

        SystemServices* currentOverride()
        {
            return overrides.empty() ? nullptr : overrides.back().get();
        }
    }

    SystemServices& SystemServices::instance()
    {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (auto* active = currentOverride())
        {
            return *active;
        }

        if (!defaultServices)
        {
            defaultServices = createPlatformServices();
        }

        return *defaultServices;
    }

    namespace testing
    {
        ScopedSystemServicesOverride::ScopedSystemServicesOverride(std::unique_ptr<SystemServices> replacement)
        {
            if (!replacement)
            {
                throw std::invalid_argument("replacement must not be null");
            }

            override_ = replacement.get();

            std::lock_guard<std::mutex> lock(instanceMutex);
            overrides.push_back(std::move(replacement));
        }

        ScopedSystemServicesOverride::~ScopedSystemServicesOverride()
        {
            std::lock_guard<std::mutex> lock(instanceMutex);

            const auto it = std::find_if(overrides.rbegin(), overrides.rend(), [this](const auto& candidate) {
                return candidate.get() == override_;
            });

            if (it != overrides.rend())
            {
                overrides.erase(std::next(it).base());
            }

            override_ = nullptr;
        }
    }
}

