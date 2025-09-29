#include "catch.hpp"

#include "Platform/SystemAppearance.h"

TEST_CASE("preferred color scheme returns a valid value", "[platform][appearance]")
{
    const auto scheme = npp::platform::preferredColorScheme();
    REQUIRE(
        scheme == npp::platform::PreferredColorScheme::Unknown ||
        scheme == npp::platform::PreferredColorScheme::Light ||
        scheme == npp::platform::PreferredColorScheme::Dark);
}
