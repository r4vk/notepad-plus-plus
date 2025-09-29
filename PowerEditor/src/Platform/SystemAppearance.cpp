#include "Platform/SystemAppearance.h"

#ifdef _WIN32
#include <windows.h>
#include <winreg.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace npp::platform
{
#ifdef _WIN32
    namespace
    {
        PreferredColorScheme readAppsThemeFlag()
        {
            constexpr const wchar_t* kSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
            constexpr const wchar_t* kValueName = L"AppsUseLightTheme";

            DWORD data = 0;
            DWORD dataSize = sizeof(data);
            const LSTATUS status = ::RegGetValueW(HKEY_CURRENT_USER, kSubKey, kValueName, RRF_RT_REG_DWORD, nullptr, &data, &dataSize);
            if (status != ERROR_SUCCESS)
            {
                return PreferredColorScheme::Unknown;
            }

            return data == 0u ? PreferredColorScheme::Dark : PreferredColorScheme::Light;
        }
    }

    PreferredColorScheme preferredColorScheme()
    {
        return readAppsThemeFlag();
    }
#elif defined(__APPLE__)
    namespace
    {
        PreferredColorScheme readGlobalDomainAppearance()
        {
            PreferredColorScheme result = PreferredColorScheme::Light;

            CFStringRef key = CFSTR("AppleInterfaceStyle");
            if (CFStringRef value = static_cast<CFStringRef>(CFPreferencesCopyAppValue(key, kCFPreferencesAnyApplication)))
            {
                const Boolean equalsDark = CFStringCompare(value, CFSTR("Dark"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
                CFRelease(value);
                result = equalsDark ? PreferredColorScheme::Dark : PreferredColorScheme::Light;
            }

            return result;
        }
    }

    PreferredColorScheme preferredColorScheme()
    {
        return readGlobalDomainAppearance();
    }
#else
    PreferredColorScheme preferredColorScheme()
    {
        return PreferredColorScheme::Unknown;
    }
#endif
}
