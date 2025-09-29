#pragma once

namespace npp::platform
{
    enum class PreferredColorScheme
    {
        Unknown,
        Light,
        Dark,
    };

    PreferredColorScheme preferredColorScheme();
}
