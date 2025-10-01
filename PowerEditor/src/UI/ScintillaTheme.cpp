#include "UI/ScintillaTheme.h"

#include <algorithm>
#include <cmath>

namespace npp::ui::scintilla
{
    namespace
    {
        Color mix(const Color& a, const Color& b, float weightB) noexcept
        {
            const float clamped = std::clamp(weightB, 0.0F, 1.0F);
            const float weightA = 1.0F - clamped;
            return {
                a.red * weightA + b.red * clamped,
                a.green * weightA + b.green * clamped,
                a.blue * weightA + b.blue * clamped,
                a.alpha * weightA + b.alpha * clamped
            };
        }

        Color emphasise(const Color& base, bool dark) noexcept
        {
            const Color target = dark ? Color{0.2F, 0.2F, 0.25F, 1.0F} : Color{1.0F, 1.0F, 1.0F, 1.0F};
            return mix(base, target, dark ? 0.35F : 0.15F);
        }

        Color adjustGutterBackground(const Color& base, bool dark) noexcept
        {
            return dark ? mix(base, Color{0.08F, 0.08F, 0.1F, 1.0F}, 0.5F)
                        : mix(base, Color{0.94F, 0.94F, 0.96F, 1.0F}, 0.6F);
        }
    }

    EditorColorScheme deriveColorScheme(const ThemePalette& palette) noexcept
    {
        const bool dark = palette.variant != ThemeVariant::Light;
        const Color selectionBlend = mix(palette.accent, palette.background, dark ? 0.6F : 0.3F);

        EditorColorScheme scheme{};
        scheme.background = emphasise(palette.background, dark);
        scheme.foreground = dark ? mix(palette.foreground, Color{0.9F, 0.9F, 0.95F, 1.0F}, 0.5F)
                                 : mix(palette.foreground, Color{0.05F, 0.05F, 0.05F, 1.0F}, 0.3F);
        scheme.caret = palette.accent;
        scheme.selectionForeground = dark ? Color{0.95F, 0.95F, 0.99F, 1.0F}
                                          : Color{0.05F, 0.05F, 0.06F, 1.0F};
        scheme.selectionBackground = selectionBlend;
        scheme.lineHighlight = mix(palette.emphasis, palette.background, dark ? 0.6F : 0.25F);
        scheme.gutterBackground = adjustGutterBackground(palette.background, dark);
        scheme.gutterForeground = dark ? mix(palette.foreground, Color{0.7F, 0.7F, 0.75F, 1.0F}, 0.4F)
                                       : mix(palette.foreground, Color{0.3F, 0.3F, 0.3F, 1.0F}, 0.4F);
        scheme.darkMode = dark;
        return scheme;
    }

    EditorMetrics computeMetrics(float devicePixelRatio) noexcept
    {
        EditorMetrics metrics{};
        metrics.devicePixelRatio = std::max(devicePixelRatio, 1.0F);
        metrics.zoomLevel = static_cast<int>(std::lround((metrics.devicePixelRatio - 1.0F) * 20.0F));
        return metrics;
    }
}

