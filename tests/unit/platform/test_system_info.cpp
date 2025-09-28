#include "catch.hpp"

#include "Platform/SystemInfo.h"

TEST_CASE("System profile exposes operating system characteristics")
{
    const auto profile = npp::platform::querySystemProfile();

    REQUIRE_FALSE(profile.operatingSystem.architecture.empty());

    if (!profile.operatingSystem.name.empty())
    {
        REQUIRE_FALSE(profile.operatingSystem.name.find_first_not_of(L" \t\r\n") == std::wstring::npos);
    }
}

TEST_CASE("Display metrics provide non-negative defaults")
{
    const auto profile = npp::platform::querySystemProfile();

    REQUIRE(profile.display.monitorCount >= 0);
    REQUIRE(profile.display.primaryWidth >= 0);
    REQUIRE(profile.display.primaryHeight >= 0);
    REQUIRE(profile.display.primaryScalePercent >= 0);
}

