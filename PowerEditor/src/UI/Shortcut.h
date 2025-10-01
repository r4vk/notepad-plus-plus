#pragma once

#include "UI/Types.h"

#include <optional>

namespace npp::ui
{
    class ShortcutMap
    {
    public:
        virtual ~ShortcutMap() = default;

        virtual void bind(const CommandId& identifier, Shortcut shortcut) = 0;
        virtual std::optional<Shortcut> lookup(const CommandId& identifier) const = 0;
        virtual void remove(const CommandId& identifier) = 0;
        virtual void clear() = 0;
        virtual void importWindowsBinding(const CommandId& identifier, const std::wstring& windowsDefinition) = 0;
    };
}

