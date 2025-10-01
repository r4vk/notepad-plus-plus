#pragma once

#include "UI/Types.h"

#include <optional>
#include <vector>

namespace npp::ui
{
    enum class MenuItemType
    {
        Action,
        Separator,
        Submenu
    };

    enum class MenuRole
    {
        Standard,
        Application,
        Window,
        Help,
        Services,
        Preferences
    };

    struct MenuItemDescriptor
    {
        MenuId identifier;
        MenuItemType type{MenuItemType::Action};
        UiString title;
        std::optional<CommandId> command;
        std::vector<MenuItemDescriptor> children;
        bool enabled{true};
        bool checked{false};
        MenuRole role{MenuRole::Standard};
    };

    class MenuModel
    {
    public:
        virtual ~MenuModel() = default;

        virtual void setItems(std::vector<MenuItemDescriptor> items) = 0;
        virtual void updateState(const MenuId& identifier, bool enabled, bool checked) = 0;
        virtual void setApplicationMenu(const MenuId& identifier) = 0;
        virtual void rebuild() = 0;
    };
}

