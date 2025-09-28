#pragma once

#include "Platform/SystemServices.h"

#include <filesystem>
#include <optional>
#include <cstdint>

namespace npp::platform
{
    class FileChangePoller
    {
    public:
        FileChangePoller(std::filesystem::path path, FileWatchEvent events);

        bool detectChanges();

        const std::filesystem::path& path() const noexcept { return path_; }

    private:
        std::filesystem::path path_;
        bool trackSize_ = false;
        bool trackLastWrite_ = false;
        bool previouslyMissing_ = false;
        bool initialCheck_ = true;
        std::optional<std::uintmax_t> lastSize_;
        std::optional<std::filesystem::file_time_type> lastWrite_;
    };
}
