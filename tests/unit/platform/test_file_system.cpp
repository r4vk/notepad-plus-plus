#include "catch.hpp"

#include "Platform/FileSystem.h"
#include "Platform/PathProvider.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
    std::filesystem::path makePath(const std::wstring& value)
    {
        return value.empty() ? std::filesystem::path{} : std::filesystem::path(value);
    }

    std::wstring uniqueComponent()
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        static std::atomic_uint64_t sequence{0};
        const auto suffix = sequence.fetch_add(1, std::memory_order_relaxed);
        return L"npp_file_system_test_" + std::to_wstring(value) + L"_" + std::to_wstring(suffix);
    }

    struct ScopedDirectory
    {
        explicit ScopedDirectory(const std::wstring& base)
        {
            const auto root = makePath(base);
            path = root / makePath(uniqueComponent());
            std::filesystem::create_directories(path);
        }

        ~ScopedDirectory()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        std::filesystem::path path;
    };
}

TEST_CASE("fileSize reports byte size for regular files")
{
    using npp::platform::KnownDirectory;

    const std::wstring tempRoot = npp::platform::pathFor(KnownDirectory::Temporary);
    REQUIRE_FALSE(tempRoot.empty());

    ScopedDirectory directory(tempRoot);

    const std::filesystem::path filePath = directory.path / "size_test.txt";

    {
        std::ofstream stream(filePath, std::ios::binary);
        REQUIRE(stream.good());
        stream << "notepad++";
    }

    const auto size = npp::platform::fileSize(filePath.wstring());
    REQUIRE(size.has_value());
    REQUIRE(*size == std::string("notepad++").size());

    const auto missing = npp::platform::fileSize((directory.path / "missing.txt").wstring());
    REQUIRE_FALSE(missing.has_value());
}

TEST_CASE("copyFile duplicates files and honours failIfExists")
{
    using npp::platform::KnownDirectory;

    const std::wstring tempRoot = npp::platform::pathFor(KnownDirectory::Temporary);
    REQUIRE_FALSE(tempRoot.empty());

    ScopedDirectory directory(tempRoot);

    const std::filesystem::path sourcePath = directory.path / "source.txt";
    const std::filesystem::path destinationPath = directory.path / "destination.txt";

    {
        std::ofstream stream(sourcePath, std::ios::binary);
        REQUIRE(stream.good());
        stream << "first";
    }

    REQUIRE(npp::platform::copyFile(sourcePath.wstring(), destinationPath.wstring(), true));
    REQUIRE(npp::platform::fileExists(destinationPath.wstring()));

    auto copiedSize = npp::platform::fileSize(destinationPath.wstring());
    REQUIRE(copiedSize.has_value());
    REQUIRE(*copiedSize == std::string("first").size());

    // A second copy with failIfExists should fail while preserving the existing file.
    REQUIRE_FALSE(npp::platform::copyFile(sourcePath.wstring(), destinationPath.wstring(), true));

    {
        std::ofstream stream(sourcePath, std::ios::binary | std::ios::trunc);
        REQUIRE(stream.good());
        stream << "updated";
    }

    REQUIRE(npp::platform::copyFile(sourcePath.wstring(), destinationPath.wstring(), false));

    copiedSize = npp::platform::fileSize(destinationPath.wstring());
    REQUIRE(copiedSize.has_value());
    REQUIRE(*copiedSize == std::string("updated").size());
}
