#include "UI/ShortcutNormalization.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string_view>

namespace npp::ui
{
    namespace
    {
        std::string toLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        KeyModifier macModifierFromToken(std::string_view token)
        {
            static const std::map<std::string, KeyModifier> mapping{
                {"ctrl", KeyModifier::Command},
                {"control", KeyModifier::Command},
                {"alt", KeyModifier::Option},
                {"shift", KeyModifier::Shift},
                {"cmd", KeyModifier::Command},
                {"command", KeyModifier::Command}
            };

            const auto it = mapping.find(std::string(token));
            if (it != mapping.end())
            {
                return it->second;
            }
            if (token == "option")
            {
                return KeyModifier::Option;
            }
            return KeyModifier::None;
        }

        std::string trim(const std::string& value)
        {
            const auto notSpace = [](unsigned char ch) {
                return !std::isspace(ch);
            };

            auto start = std::find_if(value.begin(), value.end(), notSpace);
            auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
            return (start < end) ? std::string(start, end) : std::string{};
        }

        std::vector<std::string> split(const std::string& value, char delimiter)
        {
            std::vector<std::string> tokens;
            std::string token;
            std::istringstream stream(value);
            while (std::getline(stream, token, delimiter))
            {
                tokens.push_back(trim(token));
            }
            return tokens;
        }

        std::string toUpper(const std::string& value)
        {
            std::string result = value;
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            return result;
        }
    }

    Shortcut normalizeWindowsShortcut(const LegacyShortcut& legacy)
    {
        Shortcut normalized;
        const auto tokens = split(std::string(legacy.definition.begin(), legacy.definition.end()), '+');
        ShortcutChord chord;
        for (const auto& token : tokens)
        {
            const std::string lower = toLower(token);
            const KeyModifier modifier = macModifierFromToken(lower);
            if (modifier != KeyModifier::None)
            {
                chord.modifiers |= modifier;
            }
            else if (!lower.empty())
            {
                chord.key = toUpper(token);
            }
        }

        if (!chord.key.empty())
        {
            normalized.chords.push_back(std::move(chord));
        }
        return normalized;
    }

    LegacyShortcut denormalizeShortcut(const Shortcut& shortcut)
    {
        LegacyShortcut legacy;
        if (shortcut.chords.empty())
        {
            return legacy;
        }

        const auto& chord = shortcut.chords.front();
        std::wstring result;

        if (any(chord.modifiers & KeyModifier::Command))
        {
            if (!result.empty())
            {
                result.append(L"+");
            }
            result.append(L"Ctrl");
        }
        if (any(chord.modifiers & KeyModifier::Option))
        {
            if (!result.empty())
            {
                result.append(L"+");
            }
            result.append(L"Alt");
        }
        if (any(chord.modifiers & KeyModifier::Control))
        {
            if (!result.empty())
            {
                result.append(L"+");
            }
            result.append(L"Ctrl");
        }
        if (any(chord.modifiers & KeyModifier::Shift))
        {
            if (!result.empty())
            {
                result.append(L"+");
            }
            result.append(L"Shift");
        }

        if (!chord.key.empty())
        {
            if (!result.empty())
            {
                result.append(L"+");
            }
            result.append(std::wstring(chord.key.begin(), chord.key.end()));
        }

        legacy.definition = std::move(result);
        return legacy;
    }
}

