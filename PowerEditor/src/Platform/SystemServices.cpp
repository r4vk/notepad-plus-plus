#include "Platform/SystemServices.h"

#include <algorithm>
#include <deque>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

#if defined(__APPLE__)
#include <CoreServices/CoreServices.h>
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

        private:
            StubClipboardService clipboard_;
            StubPreferencesStore preferences_;
            DocumentOpenQueue documentQueue_;
            SharingCommandQueue sharingQueue_;
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

        private:
            StubClipboardService clipboard_;
            StubPreferencesStore preferences_;
            DocumentOpenQueue documentQueue_;
            SharingCommandQueue sharingQueue_;
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

