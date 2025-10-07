#pragma once

#include <cstddef>
#include <string_view>

namespace npp::core
{
    enum class LineEnding
    {
        Windows,
        Mac,
        Unix,
        Unknown,
    };

    enum class BomEncoding
    {
        None,
        Utf8,
        Utf16BE,
        Utf16LE,
    };

    struct BomInfo
    {
        BomEncoding encoding = BomEncoding::None;
        std::size_t length = 0;

        constexpr bool hasBom() const noexcept
        {
            return encoding != BomEncoding::None;
        }
    };

    LineEnding detectLineEnding(std::string_view data, LineEnding defaultEnding = LineEnding::Unknown) noexcept;

    BomInfo detectBom(std::string_view data) noexcept;

    constexpr std::size_t bomLength(BomEncoding encoding) noexcept
    {
        switch (encoding)
        {
            case BomEncoding::Utf8:
                return 3;
            case BomEncoding::Utf16BE:
            case BomEncoding::Utf16LE:
                return 2;
            case BomEncoding::None:
            default:
                return 0;
        }
    }

    inline std::string_view stripBom(std::string_view data, BomEncoding encoding) noexcept
    {
        return data.substr(bomLength(encoding));
    }
}
