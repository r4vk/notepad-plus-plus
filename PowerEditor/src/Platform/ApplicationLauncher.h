#pragma once

#include <filesystem>

namespace npp::platform
{
    enum class Browser
    {
        Default,
        Firefox,
        Chrome,
        Edge,
        InternetExplorer,
        Safari,
    };

    bool openDocumentInBrowser(Browser browser, const std::filesystem::path& documentPath);
}
