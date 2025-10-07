#include "Platform/DocumentOpenQueue.h"

#include <algorithm>

namespace npp::platform
{
    namespace
    {
        bool containsOnlyEmptyPaths(const std::vector<std::filesystem::path>& paths)
        {
            return std::all_of(paths.begin(), paths.end(), [](const auto& path) { return path.empty(); });
        }
    }

    void DocumentOpenQueue::enqueue(DocumentOpenRequest request)
    {
        if (request.paths.empty() || containsOnlyEmptyPaths(request.paths))
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(std::move(request));
    }

    std::optional<DocumentOpenRequest> DocumentOpenQueue::poll()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_.empty())
        {
            return std::nullopt;
        }

        DocumentOpenRequest request = std::move(pending_.front());
        pending_.pop_front();
        return request;
    }

    void DocumentOpenQueue::clear() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.clear();
    }

    bool DocumentOpenQueue::empty() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.empty();
    }

    std::size_t DocumentOpenQueue::size() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.size();
    }
}

