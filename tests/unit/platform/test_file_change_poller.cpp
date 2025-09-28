#include "Platform/FileChangePoller.h"

#include "catch.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace
{
    fs::path createTemporaryDirectory()
    {
        const auto base = fs::temp_directory_path();
        const auto uniqueValue = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        fs::path directory = base / fs::path("npp-file-change-poller-" + std::to_string(uniqueValue));
        fs::create_directories(directory);
        return directory;
    }

    class DirectoryGuard
    {
    public:
        explicit DirectoryGuard(fs::path directory) : directory_(std::move(directory)) {}

        ~DirectoryGuard()
        {
            std::error_code ec;
            fs::remove_all(directory_, ec);
        }

        DirectoryGuard(const DirectoryGuard&) = delete;
        DirectoryGuard& operator=(const DirectoryGuard&) = delete;

    private:
        fs::path directory_;
    };
}

TEST_CASE("FileChangePoller detects file updates", "[platform]")
{
    const fs::path directory = createTemporaryDirectory();
    DirectoryGuard guard(directory);
    const fs::path filePath = directory / "sample.txt";

    {
        std::ofstream stream(filePath);
        stream << "seed";
    }

    npp::platform::FileChangePoller poller(filePath, npp::platform::FileWatchEvent::Size | npp::platform::FileWatchEvent::LastWrite);

    REQUIRE_FALSE(poller.detectChanges());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::ofstream stream(filePath, std::ios::app);
        stream << "update";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(poller.detectChanges());
    REQUIRE_FALSE(poller.detectChanges());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    fs::remove(filePath);

    REQUIRE(poller.detectChanges());
    REQUIRE_FALSE(poller.detectChanges());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::ofstream stream(filePath);
        stream << "recreated";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(poller.detectChanges());
}
