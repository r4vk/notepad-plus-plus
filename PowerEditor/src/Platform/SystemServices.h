#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "Platform/DocumentOpenQueue.h"
#include "Platform/SharingCommandQueue.h"

namespace npp::platform
{
    class PreferencesStore
    {
    public:
        virtual ~PreferencesStore() = default;

        virtual bool setString(const std::wstring& domain, const std::wstring& key, const std::wstring& value) = 0;

        virtual std::optional<std::wstring> getString(const std::wstring& domain, const std::wstring& key) = 0;

        virtual bool setInt64(const std::wstring& domain, const std::wstring& key, std::int64_t value) = 0;

        virtual std::optional<std::int64_t> getInt64(const std::wstring& domain, const std::wstring& key) = 0;

        virtual bool setBoolean(const std::wstring& domain, const std::wstring& key, bool value) = 0;

        virtual std::optional<bool> getBoolean(const std::wstring& domain, const std::wstring& key) = 0;

        virtual bool remove(const std::wstring& domain, const std::wstring& key) = 0;

        virtual bool clearDomain(const std::wstring& domain) = 0;
    };

    enum class FileWatchEvent : std::uint32_t
    {
        None = 0u,
        FileName = 1u << 0u,
        DirectoryName = 1u << 1u,
        Attributes = 1u << 2u,
        Size = 1u << 3u,
        LastWrite = 1u << 4u,
        LastAccess = 1u << 5u,
        Creation = 1u << 6u,
        Security = 1u << 7u,
    };

    constexpr FileWatchEvent operator|(FileWatchEvent lhs, FileWatchEvent rhs) noexcept
    {
        return static_cast<FileWatchEvent>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
    }

    constexpr FileWatchEvent operator&(FileWatchEvent lhs, FileWatchEvent rhs) noexcept
    {
        return static_cast<FileWatchEvent>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
    }

    constexpr FileWatchEvent& operator|=(FileWatchEvent& lhs, FileWatchEvent rhs) noexcept
    {
        lhs = lhs | rhs;
        return lhs;
    }

    constexpr bool hasFlag(FileWatchEvent mask, FileWatchEvent flag) noexcept
    {
        return (mask & flag) != FileWatchEvent::None;
    }

    struct FileWatchOptions
    {
        bool watchSubdirectories = false;
        std::uint32_t bufferSize = 16384u;
    };

    enum class FileChangeType
    {
        Added,
        Removed,
        Modified,
        RenamedOldName,
        RenamedNewName,
    };

    struct FileChangeNotification
    {
        FileChangeType type = FileChangeType::Modified;
        std::filesystem::path path;
    };

    class ClipboardService
    {
    public:
        virtual ~ClipboardService() = default;

        virtual bool setText(const std::wstring& text) = 0;

        virtual std::optional<std::wstring> getText() = 0;
    };

    class FileWatcher
    {
    public:
        virtual ~FileWatcher() = default;

        virtual void start() {}

        virtual void stop() noexcept {}

        virtual bool watch(const std::filesystem::path& directory, FileWatchEvent events, const FileWatchOptions& options = FileWatchOptions{}) = 0;

        virtual std::optional<FileChangeNotification> poll() = 0;
    };

    class SystemServices
    {
    public:
        virtual ~SystemServices() = default;

        virtual ClipboardService& clipboard() = 0;

        virtual std::unique_ptr<FileWatcher> createFileWatcher() = 0;

        virtual PreferencesStore& preferences() = 0;

        virtual DocumentOpenQueue& documentOpenQueue() = 0;

        virtual SharingCommandQueue& sharingCommands() = 0;

        static SystemServices& instance();
    };

    namespace testing
    {
        class ScopedSystemServicesOverride
        {
        public:
            explicit ScopedSystemServicesOverride(std::unique_ptr<SystemServices> replacement);
            ~ScopedSystemServicesOverride();

            ScopedSystemServicesOverride(const ScopedSystemServicesOverride&) = delete;
            ScopedSystemServicesOverride& operator=(const ScopedSystemServicesOverride&) = delete;
            ScopedSystemServicesOverride(ScopedSystemServicesOverride&&) noexcept = delete;
            ScopedSystemServicesOverride& operator=(ScopedSystemServicesOverride&&) noexcept = delete;

        private:
            SystemServices* override_ = nullptr;
        };
    }
}

