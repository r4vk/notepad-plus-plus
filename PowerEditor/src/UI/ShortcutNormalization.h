#pragma once

#include "UI/Shortcut.h"

namespace npp::ui
{
    struct LegacyShortcut
    {
        std::wstring definition;
    };

    Shortcut normalizeWindowsShortcut(const LegacyShortcut& legacy);

    LegacyShortcut denormalizeShortcut(const Shortcut& shortcut);
}

