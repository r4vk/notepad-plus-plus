#include "Core/TextFileAnalyzer.h"

#include <array>

namespace npp::core
{
    LineEnding detectLineEnding(std::string_view data, LineEnding defaultEnding) noexcept
    {
        if (data.empty())
            return defaultEnding;

        for (std::size_t i = 0; i < data.size(); ++i)
        {
            const char ch = data[i];
            if (ch == '\r')
            {
                const bool followedByLf = (i + 1 < data.size()) && data[i + 1] == '\n';
                return followedByLf ? LineEnding::Windows : LineEnding::Mac;
            }

            if (ch == '\n')
                return LineEnding::Unix;
        }

        return defaultEnding;
    }

    BomInfo detectBom(std::string_view data) noexcept
    {
        constexpr std::array<unsigned char, 3> utf8Bom{0xEF, 0xBB, 0xBF};
        constexpr std::array<unsigned char, 2> utf16beBom{0xFE, 0xFF};
        constexpr std::array<unsigned char, 2> utf16leBom{0xFF, 0xFE};

        const auto beginsWith = [](std::string_view text, auto bom) noexcept -> bool
        {
            if (text.size() < bom.size())
                return false;

            for (std::size_t i = 0; i < bom.size(); ++i)
            {
                if (static_cast<unsigned char>(text[i]) != bom[i])
                    return false;
            }

            return true;
        };

        if (beginsWith(data, utf8Bom))
            return {BomEncoding::Utf8, utf8Bom.size()};

        if (beginsWith(data, utf16beBom))
            return {BomEncoding::Utf16BE, utf16beBom.size()};

        if (beginsWith(data, utf16leBom))
            return {BomEncoding::Utf16LE, utf16leBom.size()};

        return {};
    }
}
