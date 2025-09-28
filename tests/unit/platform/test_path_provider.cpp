#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "Platform/FileSystem.h"
#include "Platform/PathProvider.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    std::wstring uniqueDirectoryComponent()
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        static std::atomic_uint64_t sequence{0};
        const auto suffix = sequence.fetch_add(1, std::memory_order_relaxed);
        return L"npp_platform_test_" + std::to_wstring(value) + L"_" + std::to_wstring(suffix);
    }

    std::filesystem::path makePath(const std::wstring& value)
    {
        return value.empty() ? std::filesystem::path{} : std::filesystem::path(value);
    }
}

TEST_CASE("Known directories provide usable paths")
{
    using npp::platform::KnownDirectory;

    const std::wstring tempDirectory = npp::platform::pathFor(KnownDirectory::Temporary);
    REQUIRE_FALSE(tempDirectory.empty());

    const std::wstring applicationSupport = npp::platform::pathFor(KnownDirectory::ApplicationSupport);
    REQUIRE_FALSE(applicationSupport.empty());

    const std::filesystem::path tempPath = makePath(tempDirectory);
    const std::filesystem::path appSupportPath = makePath(applicationSupport);

    REQUIRE(tempPath.is_absolute());
    REQUIRE(appSupportPath.is_absolute());
}

TEST_CASE("Combine path appends relative components using platform rules")
{
    using npp::platform::KnownDirectory;

    const std::wstring base = npp::platform::pathFor(KnownDirectory::Temporary);
    REQUIRE_FALSE(base.empty());

    const std::wstring component = uniqueDirectoryComponent();
    const std::wstring combined = npp::platform::combinePath(base, component);

    const std::filesystem::path expected = makePath(base) / makePath(component);
    REQUIRE(makePath(combined) == expected);
}

TEST_CASE("Combine path honours absolute components")
{
    const std::wstring base = L"ignored-base";

    const std::filesystem::path absoluteComponent = std::filesystem::absolute(makePath(uniqueDirectoryComponent()));
    const std::wstring combined = npp::platform::combinePath(base, absoluteComponent.wstring());

    REQUIRE(makePath(combined) == absoluteComponent);
}

TEST_CASE("ensureDirectoryExists creates directories recursively")
{
    using npp::platform::KnownDirectory;

    const std::wstring base = npp::platform::pathFor(KnownDirectory::Temporary);
    REQUIRE_FALSE(base.empty());

    const std::wstring parentComponent = uniqueDirectoryComponent();
    const std::wstring childComponent = uniqueDirectoryComponent();

    const std::wstring parent = npp::platform::combinePath(base, parentComponent);
    const std::wstring child = npp::platform::combinePath(parent, childComponent);

    REQUIRE(npp::platform::ensureDirectoryExists(child));
    REQUIRE(npp::platform::directoryExists(child));

    std::filesystem::remove_all(makePath(parent));
}

TEST_CASE("fileExists and removeFile operate on files")
{
    using npp::platform::KnownDirectory;

    const std::wstring base = npp::platform::pathFor(KnownDirectory::Temporary);
    REQUIRE_FALSE(base.empty());

    const std::wstring fileName = uniqueDirectoryComponent() + L".txt";
    const std::wstring filePath = npp::platform::combinePath(base, fileName);

    {
        std::ofstream stream(makePath(filePath));
        REQUIRE(stream.good());
    }

    REQUIRE(npp::platform::fileExists(filePath));
    REQUIRE(npp::platform::removeFile(filePath));
    REQUIRE_FALSE(npp::platform::fileExists(filePath));
}
