#pragma once

#include "UI/Docking.h"
#include "UI/Types.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace npp::ui
{
    struct WindowEventHandlers
    {
        std::function<void()> onWillClose;
        std::function<void()> onThemeChanged;
        std::function<void()> onFocusGained;
    };

    class Window
    {
    public:
        virtual ~Window() = default;

        virtual void show() = 0;
        virtual void hide() = 0;
        virtual void requestUserAttention() = 0;
        virtual void setTitle(const UiString& title) = 0;
        virtual UiString title() const = 0;
        virtual void setEventHandlers(WindowEventHandlers handlers) = 0;
        virtual void setDockingHost(std::shared_ptr<DockingHost> host) = 0;
        virtual DockingHost* dockingHost() const noexcept = 0;
        virtual void setMinimumContentSize(std::uint32_t width, std::uint32_t height) = 0;
    };
}
