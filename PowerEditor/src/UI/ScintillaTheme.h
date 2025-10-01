#pragma once

#include "UI/Theme.h"

#include <cstdint>

namespace npp::ui::scintilla
{
    struct EditorColorScheme
    {
        Color background{};
        Color foreground{};
        Color caret{};
        Color selectionForeground{};
        Color selectionBackground{};
        Color lineHighlight{};
        Color gutterBackground{};
        Color gutterForeground{};
        bool darkMode{false};
    };

    struct EditorMetrics
    {
        float devicePixelRatio{1.0F};
        int zoomLevel{0};
    };

    EditorColorScheme deriveColorScheme(const ThemePalette& palette) noexcept;

    EditorMetrics computeMetrics(float devicePixelRatio) noexcept;

    constexpr std::uint32_t toRGBA(const Color& color) noexcept
    {
        const auto clampComponent = [](float component) -> std::uint32_t {
            if (component <= 0.0F)
            {
                return 0U;
            }
            if (component >= 1.0F)
            {
                return 255U;
            }
            return static_cast<std::uint32_t>(component * 255.0F + 0.5F);
        };

        const std::uint32_t red = clampComponent(color.red);
        const std::uint32_t green = clampComponent(color.green);
        const std::uint32_t blue = clampComponent(color.blue);
        const std::uint32_t alpha = clampComponent(color.alpha);

        return (alpha << 24U) | (red << 16U) | (green << 8U) | blue;
    }
}

