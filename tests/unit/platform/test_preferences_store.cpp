#include "catch.hpp"

#include "Platform/SystemServices.h"

#include <string>

TEST_CASE("preferences store supports primitive types", "[platform][preferences]")
{
    auto& preferences = npp::platform::SystemServices::instance().preferences();
    const std::wstring domain = L"unit.test.preferences";

    preferences.clearDomain(domain);

    SECTION("strings round trip")
    {
        CHECK_FALSE(preferences.getString(domain, L"greeting").has_value());
        REQUIRE(preferences.setString(domain, L"greeting", L"hello world"));
        const auto value = preferences.getString(domain, L"greeting");
        REQUIRE(value.has_value());
        CHECK(*value == L"hello world");
    }

    SECTION("integers round trip")
    {
        CHECK_FALSE(preferences.getInt64(domain, L"launch-count").has_value());
        REQUIRE(preferences.setInt64(domain, L"launch-count", 42));
        const auto value = preferences.getInt64(domain, L"launch-count");
        REQUIRE(value.has_value());
        CHECK(*value == 42);
    }

    SECTION("booleans round trip")
    {
        CHECK_FALSE(preferences.getBoolean(domain, L"welcome-enabled").has_value());
        REQUIRE(preferences.setBoolean(domain, L"welcome-enabled", true));
        const auto value = preferences.getBoolean(domain, L"welcome-enabled");
        REQUIRE(value.has_value());
        CHECK(*value);
    }

    SECTION("type mismatches return nullopt")
    {
        REQUIRE(preferences.setString(domain, L"mixed", L"value"));
        CHECK_FALSE(preferences.getInt64(domain, L"mixed").has_value());
        CHECK_FALSE(preferences.getBoolean(domain, L"mixed").has_value());
    }

    SECTION("removing keys")
    {
        REQUIRE(preferences.setBoolean(domain, L"temporary", true));
        CHECK(preferences.remove(domain, L"temporary"));
        CHECK_FALSE(preferences.remove(domain, L"temporary"));
    }

    SECTION("clearing a domain")
    {
        REQUIRE(preferences.setString(domain, L"key", L"value"));
        CHECK(preferences.clearDomain(domain));
        CHECK_FALSE(preferences.getString(domain, L"key").has_value());
    }

    preferences.clearDomain(domain);
}
