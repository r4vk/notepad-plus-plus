#pragma once

#include "UI/Types.h"

#include <functional>
#include <memory>
#include <optional>

namespace npp::ui
{
    struct CommandState
    {
        bool enabled{true};
        bool checked{false};
    };

    struct CommandDescriptor
    {
        CommandId identifier;
        UiString title;
        UiString description;
        bool checkable{false};
        Shortcut defaultShortcut{};
        std::function<void()> action;
    };

    class Command
    {
    public:
        virtual ~Command() = default;

        virtual const CommandId& identifier() const noexcept = 0;
        virtual const UiString& title() const noexcept = 0;
        virtual const UiString& description() const noexcept = 0;
        virtual CommandState state() const noexcept = 0;
        virtual void setState(const CommandState& newState) = 0;
        virtual void trigger() = 0;
    };

    class CommandRegistry
    {
    public:
        virtual ~CommandRegistry() = default;

        virtual std::shared_ptr<Command> registerCommand(CommandDescriptor descriptor) = 0;
        virtual std::shared_ptr<Command> find(const CommandId& identifier) const noexcept = 0;
        virtual void unregister(const CommandId& identifier) = 0;
        virtual void clear() = 0;
    };
}

