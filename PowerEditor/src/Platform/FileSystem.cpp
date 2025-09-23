#include "Platform/FileSystem.h"

#include <filesystem>
#include <optional>
#include <system_error>

#ifdef _WIN32
    #include <Windows.h>
#endif

namespace npp::platform
{
    namespace
    {
        std::filesystem::path toPath(const std::wstring& value)
        {
            if (value.empty())
            {
                return {};
            }

            return std::filesystem::path(value);
        }
    }

    std::wstring combinePath(const std::wstring& base, const std::wstring& component)
    {
        try
        {
            const std::filesystem::path basePath = toPath(base);
            const std::filesystem::path componentPath = toPath(component);

            std::filesystem::path result;
            if (base.empty())
            {
                result = componentPath;
            }
            else if (component.empty())
            {
                result = basePath;
            }
            else if (componentPath.is_absolute())
            {
                result = componentPath;
            }
            else
            {
                result = basePath / componentPath;
            }

            return result.make_preferred().wstring();
        }
        catch (const std::filesystem::filesystem_error&)
        {
            std::wstring fallback = base;
            if (!fallback.empty() && !component.empty())
            {
                const wchar_t last = fallback.back();
                if (last != L'/' && last != L'\\')
                {
                    fallback += L'\\';
                }
            }

            fallback += component;
            return fallback;
        }
    }

    bool directoryExists(const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }

        try
        {
            const auto fsPath = toPath(path);
            std::error_code ec;
            const auto status = std::filesystem::status(fsPath, ec);
            if (ec)
            {
                return false;
            }

            return std::filesystem::is_directory(status);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }
    }

    bool fileExists(const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }

        try
        {
            const auto fsPath = toPath(path);
            std::error_code ec;
            const auto status = std::filesystem::status(fsPath, ec);
            if (ec)
            {
                return false;
            }

            return std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }
    }

    bool removeFile(const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }

        try
        {
            const auto fsPath = toPath(path);
            std::error_code ec;
            const auto status = std::filesystem::status(fsPath, ec);
            if (ec || (!std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status)))
            {
                return false;
            }

            return std::filesystem::remove(fsPath, ec);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }
    }

    std::optional<std::uintmax_t> fileSize(const std::wstring& path)
    {
        if (path.empty())
        {
            return std::nullopt;
        }

        try
        {
            const auto fsPath = toPath(path);
            std::error_code ec;
            const auto size = std::filesystem::file_size(fsPath, ec);
            if (ec)
            {
                return std::nullopt;
            }

            return size;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return std::nullopt;
        }
    }

    bool copyFile(const std::wstring& source, const std::wstring& destination, bool failIfExists)
    {
        if (source.empty() || destination.empty())
        {
            return false;
        }

#ifdef _WIN32
        return ::CopyFileW(source.c_str(), destination.c_str(), failIfExists ? TRUE : FALSE) != FALSE;
#else
        try
        {
            const auto srcPath = toPath(source);
            const auto dstPath = toPath(destination);
            const auto options = failIfExists ? std::filesystem::copy_options::none : std::filesystem::copy_options::overwrite_existing;
            std::error_code ec;
            const bool copied = std::filesystem::copy_file(srcPath, dstPath, options, ec);
            return copied && !ec;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }
#endif
    }
}
