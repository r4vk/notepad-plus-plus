#include "catch.hpp"

#include "Platform/DocumentOpenQueue.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path operator""_p(const char* literal, std::size_t length)
    {
        return std::filesystem::path(std::string{literal, length});
    }
}

TEST_CASE("DocumentOpenQueue ignores empty batches", "[platform][document-open]")
{
    npp::platform::DocumentOpenQueue queue;

    npp::platform::DocumentOpenRequest emptyRequest{};
    queue.enqueue(emptyRequest);

    REQUIRE(queue.empty());

    npp::platform::DocumentOpenRequest blankPath{{std::filesystem::path{}}, npp::platform::DocumentOpenSource::DragAndDrop};
    queue.enqueue(blankPath);

    REQUIRE(queue.empty());
}

TEST_CASE("DocumentOpenQueue preserves FIFO order", "[platform][document-open]")
{
    npp::platform::DocumentOpenQueue queue;

    npp::platform::DocumentOpenRequest first{{"/tmp/a.txt"_p, "/tmp/b.txt"_p}, npp::platform::DocumentOpenSource::Finder};
    npp::platform::DocumentOpenRequest second{{"/tmp/c.txt"_p}, npp::platform::DocumentOpenSource::DragAndDrop};

    queue.enqueue(std::move(first));
    queue.enqueue(std::move(second));

    REQUIRE(queue.size() == 2);

    auto request1 = queue.poll();
    REQUIRE(request1.has_value());
    REQUIRE(request1->paths.size() == 2);
    REQUIRE(request1->paths[0] == "/tmp/a.txt"_p);
    REQUIRE(request1->paths[1] == "/tmp/b.txt"_p);
    REQUIRE(request1->source == npp::platform::DocumentOpenSource::Finder);

    auto request2 = queue.poll();
    REQUIRE(request2.has_value());
    REQUIRE(request2->paths.size() == 1);
    REQUIRE(request2->paths[0] == "/tmp/c.txt"_p);
    REQUIRE(request2->source == npp::platform::DocumentOpenSource::DragAndDrop);

    REQUIRE(queue.empty());
    REQUIRE_FALSE(queue.poll().has_value());
}

