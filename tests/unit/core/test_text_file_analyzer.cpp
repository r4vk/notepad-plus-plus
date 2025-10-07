#include "catch.hpp"

#include <string>
#include <string_view>
#include <vector>

#include "Core/TextFileAnalyzer.h"

using npp::core::BomEncoding;
using npp::core::BomInfo;
using npp::core::LineEnding;

namespace
{
    std::string makeStringFromBytes(const std::vector<unsigned char>& bytes)
    {
        return std::string(bytes.begin(), bytes.end());
    }
}

TEST_CASE("Line ending detection handles common formats")
{
    using npp::core::detectLineEnding;

    SECTION("Windows CRLF")
    {
        const std::string content = "first\r\nsecond\r\n";
        CHECK(detectLineEnding(content, LineEnding::Unknown) == LineEnding::Windows);
    }

    SECTION("Unix LF")
    {
        const std::string content = "line1\nline2";
        CHECK(detectLineEnding(content, LineEnding::Unknown) == LineEnding::Unix);
    }

    SECTION("Classic macOS CR")
    {
        const std::string content = "line1\rline2";
        CHECK(detectLineEnding(content, LineEnding::Unknown) == LineEnding::Mac);
    }

    SECTION("Fallback to default when no terminator is present")
    {
        const std::string content = "single line";
        CHECK(detectLineEnding(content, LineEnding::Unix) == LineEnding::Unix);
    }
}

TEST_CASE("BOM detection recognises UTF encodings")
{
    using npp::core::detectBom;

    SECTION("UTF-8 BOM")
    {
        const std::string content = makeStringFromBytes({0xEF, 0xBB, 0xBF, 'a', 'b'});
        BomInfo info = detectBom(content);
        CHECK(info.encoding == BomEncoding::Utf8);
        CHECK(info.length == 3);
    }

    SECTION("UTF-16 BE BOM")
    {
        const std::string content = makeStringFromBytes({0xFE, 0xFF, 0x00, 0x61});
        BomInfo info = detectBom(content);
        CHECK(info.encoding == BomEncoding::Utf16BE);
        CHECK(info.length == 2);
    }

    SECTION("UTF-16 LE BOM")
    {
        const std::string content = makeStringFromBytes({0xFF, 0xFE, 0x61, 0x00});
        BomInfo info = detectBom(content);
        CHECK(info.encoding == BomEncoding::Utf16LE);
        CHECK(info.length == 2);
    }

    SECTION("No BOM present")
    {
        const std::string content = "plain";
        BomInfo info = detectBom(content);
        CHECK(info.encoding == BomEncoding::None);
        CHECK(info.length == 0);
    }
}

TEST_CASE("BOM stripping returns view without prefix")
{
    using npp::core::detectBom;
    using npp::core::stripBom;

    const std::string content = makeStringFromBytes({0xEF, 0xBB, 0xBF, 'n', 'p', 'p'});
    const BomInfo info = detectBom(content);
    REQUIRE(info.encoding == BomEncoding::Utf8);

    const auto stripped = stripBom(std::string_view(content), info.encoding);
    CHECK(stripped == std::string_view("npp"));
}
