#include "catch.hpp"

#include "UI/Factory.h"
#include "UI/QtBackend.h"

TEST_CASE("Qt backend registers warning stub when Qt is unavailable")
{
    npp::ui::Factory::resetBackend();
    npp::ui::registerQtBackend();

    REQUIRE_FALSE(npp::ui::isQtBackendAvailable());

    const npp::ui::ApplicationConfig config{
        L"Notepad++",
        L"Don Ho",
        {L"/tmp", L"/tmp/translations", L"/tmp/work"},
        {},
        npp::ui::ActivationPolicy::Regular};

    const auto result = npp::ui::Factory::create(config);
    REQUIRE_FALSE(result.success());
    REQUIRE_FALSE(result.warnings.empty());
}

