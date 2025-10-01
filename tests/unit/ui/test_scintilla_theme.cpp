#include "catch.hpp"

#include "UI/ScintillaTheme.h"

TEST_CASE("Scintilla theme derives colours for light palette")
{
    npp::ui::ThemePalette palette{};
    palette.variant = npp::ui::ThemeVariant::Light;
    palette.background = {0.98F, 0.98F, 0.99F, 1.0F};
    palette.foreground = {0.09F, 0.09F, 0.1F, 1.0F};
    palette.accent = {0.2F, 0.45F, 0.95F, 1.0F};
    palette.emphasis = {0.85F, 0.92F, 1.0F, 1.0F};

    const auto scheme = npp::ui::scintilla::deriveColorScheme(palette);
    REQUIRE_FALSE(scheme.darkMode);
    REQUIRE(scheme.background.red > scheme.foreground.red);
    REQUIRE(scheme.selectionBackground.blue != Approx(scheme.background.blue).margin(0.001F));
    REQUIRE(scheme.selectionForeground.red < 0.2F);
}

TEST_CASE("Scintilla theme derives colours for dark palette")
{
    npp::ui::ThemePalette palette{};
    palette.variant = npp::ui::ThemeVariant::Dark;
    palette.background = {0.08F, 0.08F, 0.09F, 1.0F};
    palette.foreground = {0.82F, 0.83F, 0.85F, 1.0F};
    palette.accent = {0.42F, 0.64F, 0.98F, 1.0F};
    palette.emphasis = {0.16F, 0.18F, 0.22F, 1.0F};

    const auto scheme = npp::ui::scintilla::deriveColorScheme(palette);
    REQUIRE(scheme.darkMode);
    REQUIRE(scheme.background.red > palette.background.red);
    REQUIRE(scheme.gutterForeground.blue > 0.5F);
    REQUIRE(scheme.selectionForeground.red > 0.5F);
}

TEST_CASE("Scintilla metrics compute zoom level from device pixel ratio")
{
    const auto metricsStandard = npp::ui::scintilla::computeMetrics(1.0F);
    REQUIRE(metricsStandard.zoomLevel == 0);

    const auto metricsRetina = npp::ui::scintilla::computeMetrics(2.0F);
    REQUIRE(metricsRetina.zoomLevel == 20);

    const auto metricsLarge = npp::ui::scintilla::computeMetrics(2.7F);
    REQUIRE(metricsLarge.zoomLevel == 34);
}
