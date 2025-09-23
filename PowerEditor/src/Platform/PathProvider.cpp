#include "Platform/PathProvider.h"

#ifdef _WIN32
    #include <ShlObj.h>
    #include <Windows.h>
#else
    #include <cstdlib>
    #include <filesystem>
    #include <pwd.h>
    #include <unistd.h>
#endif

namespace npp::platform
{
    namespace
    {
    #ifdef _WIN32
        std::wstring queryShellFolder(int csidl)
        {
            wchar_t buffer[MAX_PATH]{};
            if (SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, buffer) == S_OK)
            {
                return buffer;
            }
            return {};
        }

        std::wstring queryTemporaryDirectory()
        {
            wchar_t buffer[MAX_PATH]{};
            const DWORD length = ::GetTempPathW(MAX_PATH, buffer);
            if (length == 0)
            {
                return {};
            }

            return std::wstring(buffer, buffer + length);
        }
    #else
        std::filesystem::path fetchHomePath()
        {
            if (const char* home = std::getenv("HOME"); home && *home)
            {
                return std::filesystem::path(home);
            }

            if (const passwd* pwd = ::getpwuid(::getuid()); pwd && pwd->pw_dir)
            {
                return std::filesystem::path(pwd->pw_dir);
            }

            return {};
        }

        std::filesystem::path fetchTemporaryDirectory()
        {
            const char* const candidates[] = { "TMPDIR", "TMP", "TEMP" };
            for (const char* name : candidates)
            {
                if (const char* value = std::getenv(name); value && *value)
                {
                    return std::filesystem::path(value);
                }
            }

        #if defined(__APPLE__)
            return std::filesystem::path("/tmp");
        #else
            try
            {
                return std::filesystem::temp_directory_path();
            }
            catch (const std::filesystem::filesystem_error&)
            {
                return {};
            }
        #endif
        }
    #endif
    } // namespace

    std::wstring pathFor(KnownDirectory directory)
    {
    #ifdef _WIN32
        switch (directory)
        {
            case KnownDirectory::UserHome:
                return queryShellFolder(CSIDL_PROFILE);
            case KnownDirectory::RoamingData:
                return queryShellFolder(CSIDL_APPDATA);
            case KnownDirectory::LocalData:
                return queryShellFolder(CSIDL_LOCAL_APPDATA);
            case KnownDirectory::ProgramFiles:
                return queryShellFolder(CSIDL_PROGRAM_FILES);
            case KnownDirectory::ApplicationSupport:
                return queryShellFolder(CSIDL_APPDATA);
            case KnownDirectory::Temporary:
                return queryTemporaryDirectory();
        }
        return {};
    #else
        const auto home = fetchHomePath();
        if (home.empty())
        {
            return {};
        }

        switch (directory)
        {
            case KnownDirectory::UserHome:
                return home.wstring();
            case KnownDirectory::RoamingData:
    #if defined(__APPLE__)
                return (home / "Library" / "Application Support").wstring();
    #else
                return (home / ".config").wstring();
    #endif
            case KnownDirectory::LocalData:
    #if defined(__APPLE__)
                return (home / "Library" / "Caches").wstring();
    #else
                return (home / ".local" / "share").wstring();
    #endif
            case KnownDirectory::ProgramFiles:
    #if defined(__APPLE__)
                return std::filesystem::path("/Applications").wstring();
    #else
                return (home / ".local" / "share").wstring();
    #endif
            case KnownDirectory::ApplicationSupport:
    #if defined(__APPLE__)
                return (home / "Library" / "Application Support").wstring();
    #else
                return (home / ".config").wstring();
    #endif
            case KnownDirectory::Temporary:
            {
                const auto temporaryDirectory = fetchTemporaryDirectory();
                return temporaryDirectory.empty() ? std::wstring{} : temporaryDirectory.wstring();
            }
        }
        return {};
    #endif
    }

    bool ensureDirectoryExists(const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }

    #ifdef _WIN32
        const DWORD attributes = ::GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            return true;
        }

        const int result = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
        if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS)
        {
            return true;
        }

        if (result == ERROR_FILE_EXISTS)
        {
            const DWORD existingAttributes = ::GetFileAttributesW(path.c_str());
            return existingAttributes != INVALID_FILE_ATTRIBUTES && (existingAttributes & FILE_ATTRIBUTE_DIRECTORY);
        }

        return false;
    #else
        try
        {
            const std::filesystem::path fsPath(path);
            if (std::filesystem::exists(fsPath))
            {
                return std::filesystem::is_directory(fsPath);
            }

            return std::filesystem::create_directories(fsPath);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }
    #endif
    }
} // namespace npp::platform
