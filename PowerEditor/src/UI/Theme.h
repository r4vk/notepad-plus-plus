#pragma once

#include "UI/Types.h"

#include <vector>

namespace npp::ui
{
    struct ThemePalette
    {
        ThemeVariant variant{ThemeVariant::Light};
        Color accent{};
        Color background{};
        Color foreground{};
        Color emphasis{};
    };

    class ThemeObserver
    {
    public:
        virtual ~ThemeObserver() = default;
        virtual void onThemeChanged(const ThemePalette& palette) = 0;
    };

    class ThemeProvider
    {
    public:
        virtual ~ThemeProvider() = default;

        virtual ThemePalette palette() const = 0;
        virtual void addObserver(ThemeObserver& observer) = 0;
        virtual void removeObserver(ThemeObserver& observer) = 0;
    };
}

