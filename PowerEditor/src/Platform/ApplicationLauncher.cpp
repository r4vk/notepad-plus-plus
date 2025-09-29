#include "Platform/ApplicationLauncher.h"

#include <optional>
#include <string>

#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#include <winreg.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace npp::platform
{
#ifdef _WIN32
    namespace
    {
        std::optional<std::wstring> findExecutableInAppPaths(const wchar_t* executableName)
        {
            std::wstring key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";
            key += executableName;

            HKEY handle = nullptr;
            if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ, &handle) != ERROR_SUCCESS)
            {
                return std::nullopt;
            }

            wchar_t buffer[MAX_PATH] = {0};
            DWORD bufferSize = sizeof(buffer);
            DWORD valueType = 0;
            const LSTATUS status = ::RegQueryValueExW(handle, L"", nullptr, &valueType, reinterpret_cast<LPBYTE>(buffer), &bufferSize);
            ::RegCloseKey(handle);

            if (status != ERROR_SUCCESS || valueType != REG_SZ || buffer[0] == L'\0')
            {
                return std::nullopt;
            }

            return std::wstring(buffer);
        }

        bool launchWithExecutable(const std::wstring& executable, const std::wstring& parameters)
        {
            const INT_PTR result = reinterpret_cast<INT_PTR>(::ShellExecuteW(nullptr, L"open", executable.c_str(), parameters.c_str(), nullptr, SW_SHOWNORMAL));
            return result > 32;
        }

        bool launchEdgeFallback(const std::wstring& document)
        {
            const INT_PTR result = reinterpret_cast<INT_PTR>(::ShellExecuteW(nullptr,
                L"open",
                L"shell:Appsfolder\\Microsoft.MicrosoftEdge_8wekyb3d8bbwe!MicrosoftEdge",
                document.c_str(),
                nullptr,
                SW_SHOW));
            return result > 32;
        }

        std::wstring quote(const std::filesystem::path& path)
        {
            std::wstring quoted;
            quoted.reserve(path.native().size() + 2);
            quoted.push_back(L'"');
            quoted += path.native();
            quoted.push_back(L'"');
            return quoted;
        }
    }

    bool openDocumentInBrowser(Browser browser, const std::filesystem::path& documentPath)
    {
        const std::wstring quotedDocument = quote(documentPath);
        const std::wstring nativeDocument = documentPath.native();

        auto tryLaunch = [&](const wchar_t* executableName) -> bool
        {
            if (auto executable = findExecutableInAppPaths(executableName))
            {
                return launchWithExecutable(*executable, quotedDocument);
            }
            return false;
        };

        switch (browser)
        {
        case Browser::Firefox:
            if (tryLaunch(L"firefox.exe"))
                return true;
            break;

        case Browser::Chrome:
            if (tryLaunch(L"chrome.exe"))
                return true;
            break;

        case Browser::Edge:
            if (tryLaunch(L"msedge.exe"))
                return true;
            if (launchEdgeFallback(nativeDocument))
                return true;
            break;

        case Browser::InternetExplorer:
            if (tryLaunch(L"iexplore.exe"))
                return true;
            break;

        case Browser::Safari:
            // Not available on Windows – fall through to default handler
            break;

        case Browser::Default:
            break;
        }

        const INT_PTR result = reinterpret_cast<INT_PTR>(::ShellExecuteW(nullptr,
            L"open",
            nativeDocument.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL));
        return result > 32;
    }
#elif defined(__APPLE__)
    namespace
    {
        CFURLRef createFileURL(const std::filesystem::path& path)
        {
            const std::string utf8 = path.u8string();
            return CFURLCreateFromFileSystemRepresentation(nullptr,
                reinterpret_cast<const UInt8*>(utf8.c_str()),
                utf8.length(),
                false);
        }

        bool launchWithBundleID(const char* bundleID, CFURLRef fileURL)
        {
            CFStringRef bundleString = CFStringCreateWithCString(nullptr, bundleID, kCFStringEncodingUTF8);
            if (!bundleString)
            {
                return false;
            }

            CFURLRef appURL = nullptr;
            const OSStatus status = LSFindApplicationForInfo(kLSUnknownCreator, bundleString, nullptr, nullptr, &appURL);
            CFRelease(bundleString);

            if (status != noErr || !appURL)
            {
                if (appURL)
                {
                    CFRelease(appURL);
                }
                return false;
            }

            CFArrayRef items = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&fileURL), 1, &kCFTypeArrayCallBacks);
            if (!items)
            {
                CFRelease(appURL);
                return false;
            }

            LSLaunchURLSpec spec{};
            spec.appURL = appURL;
            spec.itemURLs = items;
            spec.launchFlags = kLSLaunchDefaults;

            const OSStatus launchStatus = LSOpenFromURLSpec(&spec, nullptr);

            CFRelease(items);
            CFRelease(appURL);
            return launchStatus == noErr;
        }
    }

    bool openDocumentInBrowser(Browser browser, const std::filesystem::path& documentPath)
    {
        CFURLRef fileURL = createFileURL(documentPath);
        if (!fileURL)
        {
            return false;
        }

        auto tryBundle = [&](const char* bundleID) -> bool
        {
            return launchWithBundleID(bundleID, fileURL);
        };

        bool launched = false;
        switch (browser)
        {
        case Browser::Safari:
            launched = tryBundle("com.apple.Safari");
            break;
        case Browser::Chrome:
            launched = tryBundle("com.google.Chrome");
            break;
        case Browser::Firefox:
            launched = tryBundle("org.mozilla.firefox");
            break;
        case Browser::Edge:
            launched = tryBundle("com.microsoft.edgemac");
            break;
        case Browser::InternetExplorer:
            // no-op on macOS – fall back to default handler
            launched = false;
            break;
        case Browser::Default:
            launched = false;
            break;
        }

        if (!launched)
        {
            const OSStatus status = LSOpenCFURLRef(fileURL, nullptr);
            launched = (status == noErr);
        }

        CFRelease(fileURL);
        return launched;
    }
#else
    bool openDocumentInBrowser(Browser, const std::filesystem::path&)
    {
        return false;
    }
#endif
}
