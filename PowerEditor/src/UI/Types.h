#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace npp::ui
{
    using UiString = std::wstring;
    using CommandId = std::string;
    using MenuId = std::string;
    using ToolbarId = std::string;
    using PanelId = std::string;

    struct Color
    {
        float red{0.0F};
        float green{0.0F};
        float blue{0.0F};
        float alpha{1.0F};
    };

    enum class ThemeVariant
    {
        Light,
        Dark,
        HighContrast
    };

    enum class ActivationPolicy
    {
        Regular,
        Accessory,
        Prohibited
    };

    enum class DockArea
    {
        Left,
        Right,
        Top,
        Bottom,
        Floating
    };

    enum class KeyModifier : std::uint8_t
    {
        None = 0,
        Command = 1U << 0,
        Option = 1U << 1,
        Control = 1U << 2,
        Shift = 1U << 3
    };

    constexpr KeyModifier operator|(KeyModifier lhs, KeyModifier rhs) noexcept
    {
        return static_cast<KeyModifier>(static_cast<std::uint8_t>(lhs) |
                                        static_cast<std::uint8_t>(rhs));
    }

    constexpr KeyModifier operator&(KeyModifier lhs, KeyModifier rhs) noexcept
    {
        return static_cast<KeyModifier>(static_cast<std::uint8_t>(lhs) &
                                        static_cast<std::uint8_t>(rhs));
    }

    constexpr KeyModifier operator~(KeyModifier value) noexcept
    {
        return static_cast<KeyModifier>(~static_cast<std::uint8_t>(value));
    }

    inline KeyModifier& operator|=(KeyModifier& lhs, KeyModifier rhs) noexcept
    {
        lhs = lhs | rhs;
        return lhs;
    }

    constexpr KeyModifier& operator|=(KeyModifier& lhs, int rhs) noexcept = delete;

    inline KeyModifier& operator&=(KeyModifier& lhs, KeyModifier rhs) noexcept
    {
        lhs = lhs & rhs;
        return lhs;
    }

    constexpr bool any(KeyModifier value) noexcept
    {
        return static_cast<std::uint8_t>(value) != 0U;
    }

    struct ShortcutChord
    {
        KeyModifier modifiers{KeyModifier::None};
        std::string key; // UTF-8 reprezentacja przycisku (np. "F", "Enter").
    };

    struct Shortcut
    {
        std::vector<ShortcutChord> chords;
    };

    struct UiToken
    {
        std::string value;

        bool operator==(const UiToken& other) const noexcept
        {
            return value == other.value;
        }
    };
}
