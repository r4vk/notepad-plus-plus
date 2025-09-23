#include "Platform/FileSystem.h"

#include <filesystem>
#include <system_error>

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
}
