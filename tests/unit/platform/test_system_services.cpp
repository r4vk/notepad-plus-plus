#include "catch.hpp"

#include "Platform/SystemServices.h"

TEST_CASE("FileWatchEvent bitmask operations compose values")
{
    using npp::platform::FileWatchEvent;

    const auto combined = FileWatchEvent::FileName | FileWatchEvent::DirectoryName | FileWatchEvent::LastWrite;

    REQUIRE(npp::platform::hasFlag(combined, FileWatchEvent::FileName));
    REQUIRE(npp::platform::hasFlag(combined, FileWatchEvent::DirectoryName));
    REQUIRE(npp::platform::hasFlag(combined, FileWatchEvent::LastWrite));
    REQUIRE_FALSE(npp::platform::hasFlag(combined, FileWatchEvent::Security));
}

#ifndef _WIN32
TEST_CASE("Stub clipboard stores text in memory for non-Windows builds")
{
    auto& clipboard = npp::platform::SystemServices::instance().clipboard();

    REQUIRE(clipboard.setText(L"Hello macOS"));

    const auto roundTrip = clipboard.getText();
    REQUIRE(roundTrip.has_value());
    REQUIRE(roundTrip.value() == L"Hello macOS");
}
#endif

