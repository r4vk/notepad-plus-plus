#pragma once

#include "UI/Types.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace npp::ui
{
    struct DockLayoutSnapshot
    {
        std::string encodedState;
        UiString description;
        ThemeVariant variant{ThemeVariant::Light};
    };

    struct PanelIcon
    {
        std::wstring resourcePath;
        std::wstring symbolicName;
    };

    class DockableView
    {
    public:
        virtual ~DockableView() = default;

        virtual void requestFocus() = 0;
        virtual void setVisible(bool visible) = 0;
        virtual void onThemeChanged(ThemeVariant variant) = 0;
    };

    struct DockablePanelDescriptor
    {
        PanelId identifier;
        UiString title;
        std::optional<PanelIcon> icon;
        bool initiallyVisible{true};
        DockArea preferredArea{DockArea::Right};
        std::function<std::shared_ptr<DockableView>()> viewFactory;
    };

    class DockingHost
    {
    public:
        virtual ~DockingHost() = default;

        virtual void registerPanel(DockablePanelDescriptor descriptor) = 0;
        virtual void unregisterPanel(const PanelId& identifier) = 0;
        virtual void setLayout(const DockLayoutSnapshot& snapshot) = 0;
        virtual DockLayoutSnapshot captureLayout() const = 0;
        virtual void showPanel(const PanelId& identifier) = 0;
        virtual void hidePanel(const PanelId& identifier) = 0;
        virtual bool isPanelVisible(const PanelId& identifier) const = 0;
    };
}

