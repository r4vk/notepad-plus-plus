#include "catch.hpp"

#include "UI/ShortcutNormalization.h"

TEST_CASE("Windows shortcut is normalized to mac modifiers")
{
    const npp::ui::LegacyShortcut legacy{L"Ctrl+Alt+Shift+F"};
    const auto normalized = npp::ui::normalizeWindowsShortcut(legacy);

    REQUIRE(normalized.chords.size() == 1);
    const auto& chord = normalized.chords.front();
    REQUIRE(npp::ui::any(chord.modifiers & npp::ui::KeyModifier::Command));
    REQUIRE(npp::ui::any(chord.modifiers & npp::ui::KeyModifier::Option));
    REQUIRE(npp::ui::any(chord.modifiers & npp::ui::KeyModifier::Shift));
    REQUIRE(chord.key == "F");
}

TEST_CASE("Non modifier token becomes key")
{
    const npp::ui::LegacyShortcut legacy{L"Ctrl+K"};
    const auto normalized = npp::ui::normalizeWindowsShortcut(legacy);
    REQUIRE(normalized.chords.front().key == "K");
}

TEST_CASE("Denormalization reconstructs Windows definition")
{
    npp::ui::Shortcut shortcut;
    npp::ui::ShortcutChord chord;
    chord.modifiers |= npp::ui::KeyModifier::Command;
    chord.modifiers |= npp::ui::KeyModifier::Option;
    chord.key = "P";
    shortcut.chords.push_back(chord);

    const auto legacy = npp::ui::denormalizeShortcut(shortcut);
    REQUIRE(legacy.definition.find(L"Ctrl") != std::wstring::npos);
    REQUIRE(legacy.definition.find(L"Alt") != std::wstring::npos);
    REQUIRE(legacy.definition.find(L"P") != std::wstring::npos);
}

