#pragma once

#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace npp::platform
{
    enum class SharingCommandType
    {
        QuickLookPreview,
        ServicesMenu,
        RevealInFinder,
    };

    struct SharingCommand
    {
        SharingCommandType type = SharingCommandType::QuickLookPreview;
        std::vector<std::filesystem::path> items;
        std::optional<std::wstring> serviceIdentifier;
    };

    class SharingCommandQueue
    {
    public:
        void enqueue(SharingCommand command);

        std::optional<SharingCommand> poll();

        void clear() noexcept;

        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] std::size_t size() const noexcept;

    private:
        mutable std::mutex mutex_;
        std::deque<SharingCommand> pending_;
    };
}

