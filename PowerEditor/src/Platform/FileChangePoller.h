#pragma once

#include "Platform/SystemServices.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <cstdint>

namespace npp::platform
{
    class FileChangePoller
    {
    public:
        FileChangePoller(std::filesystem::path path, FileWatchEvent events);
        ~FileChangePoller();

        FileChangePoller(FileChangePoller&&) noexcept;
        FileChangePoller& operator=(FileChangePoller&&) noexcept;

        FileChangePoller(const FileChangePoller&) = delete;
        FileChangePoller& operator=(const FileChangePoller&) = delete;

        bool detectChanges();

        const std::filesystem::path& path() const noexcept;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
