#include "Platform/SharingCommandQueue.h"

namespace npp::platform
{
    void SharingCommandQueue::enqueue(SharingCommand command)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(std::move(command));
    }

    std::optional<SharingCommand> SharingCommandQueue::poll()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_.empty())
        {
            return std::nullopt;
        }

        SharingCommand command = std::move(pending_.front());
        pending_.pop_front();
        return command;
    }

    void SharingCommandQueue::clear() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.clear();
    }

    bool SharingCommandQueue::empty() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.empty();
    }

    std::size_t SharingCommandQueue::size() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.size();
    }
}

