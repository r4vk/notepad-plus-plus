#include "SystemInfo.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <type_traits>

#ifdef _WIN32
    #include <memory>
    #include <format>
    #include <windows.h>
#endif

namespace npp::platform
{
#ifdef _WIN32
    namespace
    {
        struct RegKeyDeleter
        {
            void operator()(HKEY key) const noexcept
            {
                if (key != nullptr)
                {
                    ::RegCloseKey(key);
                }
            }
        };

        using UniqueRegKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegKeyDeleter>;

        std::wstring readRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valueName)
        {
            DWORD dataSize = 0;
            if (::RegGetValueW(root, subKey, valueName, RRF_RT_REG_SZ, nullptr, nullptr, &dataSize) != ERROR_SUCCESS || dataSize == 0)
            {
                return {};
            }

            std::wstring buffer(dataSize / sizeof(wchar_t), L'\0');
            if (::RegGetValueW(root, subKey, valueName, RRF_RT_REG_SZ, nullptr, buffer.data(), &dataSize) != ERROR_SUCCESS)
            {
                return {};
            }

            if (!buffer.empty() && buffer.back() == L'\0')
            {
                buffer.pop_back();
            }

            return buffer;
        }

        std::wstring readRegistryVersionValue(HKEY root, const wchar_t* subKey)
        {
            std::wstring version = readRegistryString(root, subKey, L"DisplayVersion");
            if (!version.empty())
            {
                return version;
            }

            return readRegistryString(root, subKey, L"ReleaseId");
        }

        std::wstring readRegistryBuildNumber(HKEY root, const wchar_t* subKey)
        {
            DWORD dataSize = 0;
            if (::RegGetValueW(root, subKey, L"CurrentBuildNumber", RRF_RT_REG_SZ, nullptr, nullptr, &dataSize) == ERROR_SUCCESS && dataSize > 0)
            {
                std::wstring buffer(dataSize / sizeof(wchar_t), L'\0');
                if (::RegGetValueW(root, subKey, L"CurrentBuildNumber", RRF_RT_REG_SZ, nullptr, buffer.data(), &dataSize) == ERROR_SUCCESS)
                {
                    if (!buffer.empty() && buffer.back() == L'\0')
                    {
                        buffer.pop_back();
                    }

                    DWORD ubr = 0;
                    DWORD ubrSize = sizeof(ubr);
                    if (::RegGetValueW(root, subKey, L"UBR", RRF_RT_REG_DWORD, nullptr, &ubr, &ubrSize) == ERROR_SUCCESS && ubrSize == sizeof(ubr))
                    {
                        return std::format(L"{}.{}", buffer, ubr);
                    }

                    return buffer;
                }
            }

            DWORD buildNumber = 0;
            DWORD buildNumberSize = sizeof(buildNumber);
            if (::RegGetValueW(root, subKey, L"CurrentBuild", RRF_RT_REG_SZ, nullptr, nullptr, &buildNumberSize) == ERROR_SUCCESS && buildNumberSize > 0)
            {
                std::wstring buffer(buildNumberSize / sizeof(wchar_t), L'\0');
                if (::RegGetValueW(root, subKey, L"CurrentBuild", RRF_RT_REG_SZ, nullptr, buffer.data(), &buildNumberSize) == ERROR_SUCCESS)
                {
                    if (!buffer.empty() && buffer.back() == L'\0')
                    {
                        buffer.pop_back();
                    }

                    return buffer;
                }
            }

            return {};
        }

        OperatingSystemInfo queryOperatingSystemInfo()
        {
            constexpr wchar_t currentVersionKey[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

            OperatingSystemInfo info;
            info.name = readRegistryString(HKEY_LOCAL_MACHINE, currentVersionKey, L"ProductName");
            info.version = readRegistryVersionValue(HKEY_LOCAL_MACHINE, currentVersionKey);
            info.build = readRegistryBuildNumber(HKEY_LOCAL_MACHINE, currentVersionKey);

            SYSTEM_INFO systemInfo{};
            ::GetNativeSystemInfo(&systemInfo);

            switch (systemInfo.wProcessorArchitecture)
            {
                case PROCESSOR_ARCHITECTURE_AMD64:
                    info.architecture = L"64-bit";
                    break;
                case PROCESSOR_ARCHITECTURE_INTEL:
                    info.architecture = L"32-bit";
                    break;
                case PROCESSOR_ARCHITECTURE_ARM64:
                    info.architecture = L"ARM64";
                    break;
                default:
                    info.architecture = L"unknown";
                    break;
            }

            if (info.name.empty())
            {
                OSVERSIONINFOW versionInfo{};
                versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
                if (::GetVersionExW(&versionInfo))
                {
                    info.name = std::format(L"Windows {}.{}", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion);
                    if (info.version.empty())
                    {
                        info.version = std::to_wstring(versionInfo.dwBuildNumber);
                    }
                }
            }

            if (info.build.empty())
            {
                OSVERSIONINFOW versionInfo{};
                versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
                if (::GetVersionExW(&versionInfo))
                {
                    info.build = std::to_wstring(versionInfo.dwBuildNumber);
                }
            }

            return info;
        }

        DisplayMetrics queryDisplayMetrics()
        {
            DisplayMetrics metrics;

            if (HDC hdc = ::GetDC(nullptr); hdc != nullptr)
            {
                metrics.primaryWidth = ::GetDeviceCaps(hdc, HORZRES);
                metrics.primaryHeight = ::GetDeviceCaps(hdc, VERTRES);
                const int logicalDpiX = ::GetDeviceCaps(hdc, LOGPIXELSX);
                metrics.primaryScalePercent = logicalDpiX > 0 ? (logicalDpiX * 100) / 96 : 0;
                ::ReleaseDC(nullptr, hdc);
            }

            metrics.monitorCount = ::GetSystemMetrics(SM_CMONITORS);

            constexpr wchar_t displayKey[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E968-E325-11CE-BFC1-08002BE10318}";

            HKEY rawKey = nullptr;
            if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, displayKey, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &rawKey) == ERROR_SUCCESS)
            {
                UniqueRegKey displayKeyHandle(rawKey);

                DWORD subKeyCount = 0;
                if (::RegQueryInfoKeyW(displayKeyHandle.get(), nullptr, nullptr, nullptr, &subKeyCount, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
                {
                    constexpr unsigned int maximumAdapters = 4;
                    metrics.adapters.reserve(std::min<DWORD>(subKeyCount, maximumAdapters));

                    for (DWORD index = 0; index < subKeyCount; ++index)
                    {
                        if (metrics.adapters.size() >= maximumAdapters)
                        {
                            metrics.adaptersTruncated = true;
                            break;
                        }

                        wchar_t subKeyName[256] = {};
                        DWORD subKeyNameSize = static_cast<DWORD>(std::size(subKeyName));
                        if (::RegEnumKeyExW(displayKeyHandle.get(), index, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                        {
                            continue;
                        }

                        std::wstring adapterKey = displayKey;
                        adapterKey.push_back(L'\\');
                        adapterKey.append(subKeyName, subKeyNameSize);

                        DisplayAdapterInfo adapter;
                        adapter.description = readRegistryString(HKEY_LOCAL_MACHINE, adapterKey.c_str(), L"DriverDesc");
                        adapter.driverVersion = readRegistryString(HKEY_LOCAL_MACHINE, adapterKey.c_str(), L"DriverVersion");

                        if (!adapter.description.empty() || !adapter.driverVersion.empty())
                        {
                            metrics.adapters.emplace_back(std::move(adapter));
                        }
                    }
                }
            }

            return metrics;
        }
    }
#endif

    SystemProfile querySystemProfile()
    {
        SystemProfile profile;

#ifdef _WIN32
        profile.operatingSystem = queryOperatingSystemInfo();
        profile.display = queryDisplayMetrics();
#else
        profile.operatingSystem.name = L"macOS";
        profile.operatingSystem.architecture = sizeof(void*) == 8 ? L"64-bit" : L"32-bit";
#endif

        return profile;
    }
}

