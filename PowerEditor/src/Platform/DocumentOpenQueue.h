#pragma once

#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <vector>

namespace npp::platform
{
    enum class DocumentOpenSource
    {
        DragAndDrop,
        Finder,
        CommandLine,
        Other,
    };

    struct DocumentOpenRequest
    {
        std::vector<std::filesystem::path> paths;
        DocumentOpenSource source = DocumentOpenSource::Other;
    };

    class DocumentOpenQueue
    {
    public:
        void enqueue(DocumentOpenRequest request);

        std::optional<DocumentOpenRequest> poll();

        void clear() noexcept;

        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] std::size_t size() const noexcept;

    private:
        mutable std::mutex mutex_;
        std::deque<DocumentOpenRequest> pending_;
    };
}

