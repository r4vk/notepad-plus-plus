#pragma once

#include "UI/Types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace npp::ui
{
    struct TabIdentifier
    {
        std::string value;

        bool operator==(const TabIdentifier& other) const noexcept
        {
            return value == other.value;
        }
    };

    struct TabMetadata
    {
        UiString title;
        UiString tooltip;
        bool dirty{false};
        bool readOnly{false};
        bool monitoring{false};
        std::optional<UiString> badge;
    };

    struct TabCloseEvent
    {
        TabIdentifier identifier;
        bool userInitiated{false};
    };

    struct TabSelectionEvent
    {
        TabIdentifier identifier;
    };

    class TabEventSink
    {
    public:
        virtual ~TabEventSink() = default;

        virtual void onCloseRequest(const TabCloseEvent& event) = 0;
        virtual void onSelected(const TabSelectionEvent& event) = 0;
        virtual void onDragged(const TabIdentifier& from, std::uint32_t toIndex) = 0;
    };

    struct TabDescriptor
    {
        TabIdentifier identifier;
        TabMetadata metadata;
    };

    class TabManager
    {
    public:
        virtual ~TabManager() = default;

        virtual void setEventSink(TabEventSink* sink) = 0;
        virtual void setTabs(std::vector<TabDescriptor> tabs, std::optional<TabIdentifier> active) = 0;
        virtual void updateMetadata(const TabIdentifier& identifier, const TabMetadata& metadata) = 0;
        virtual void setActive(const TabIdentifier& identifier) = 0;
        virtual void remove(const TabIdentifier& identifier) = 0;
        virtual void move(const TabIdentifier& identifier, std::uint32_t newIndex) = 0;
        virtual std::uint32_t count() const noexcept = 0;
    };
}

