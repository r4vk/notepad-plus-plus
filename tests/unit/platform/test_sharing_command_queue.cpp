#include "catch.hpp"

#include "Platform/SharingCommandQueue.h"

#include <filesystem>
#include <optional>
#include <string>

using namespace std::string_literals;

TEST_CASE("sharing command queue preserves FIFO order and metadata", "[platform][sharing]")
{
    npp::platform::SharingCommandQueue queue;

    npp::platform::SharingCommand quickLook{};
    quickLook.type = npp::platform::SharingCommandType::QuickLookPreview;
    quickLook.items = {std::filesystem::path{"/Users/test/Documents/notes.md"}};

    npp::platform::SharingCommand services{};
    services.type = npp::platform::SharingCommandType::ServicesMenu;
    services.items = {std::filesystem::path{"/Users/test/Documents/report.pdf"}};
    services.serviceIdentifier = L"com.notepadplusplus.mac.share"s;

    queue.enqueue(quickLook);
    queue.enqueue(services);

    REQUIRE_FALSE(queue.empty());
    CHECK(queue.size() == 2);

    const auto first = queue.poll();
    REQUIRE(first.has_value());
    CHECK(first->type == npp::platform::SharingCommandType::QuickLookPreview);
    REQUIRE(first->items.size() == 1);
    CHECK(first->items.front() == std::filesystem::path{"/Users/test/Documents/notes.md"});
    CHECK_FALSE(first->serviceIdentifier.has_value());

    const auto second = queue.poll();
    REQUIRE(second.has_value());
    CHECK(second->type == npp::platform::SharingCommandType::ServicesMenu);
    REQUIRE(second->items.size() == 1);
    CHECK(second->items.front() == std::filesystem::path{"/Users/test/Documents/report.pdf"});
    REQUIRE(second->serviceIdentifier.has_value());
    CHECK(second->serviceIdentifier == L"com.notepadplusplus.mac.share"s);

    CHECK(queue.empty());

    queue.enqueue({npp::platform::SharingCommandType::RevealInFinder, {std::filesystem::path{"/Users/test/Desktop"}}, std::nullopt});
    CHECK(queue.size() == 1);
    queue.clear();
    CHECK(queue.empty());
}

