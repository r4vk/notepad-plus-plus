#include "catch.hpp"

#include "UI/Factory.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    class StubDockingHost final : public npp::ui::DockingHost
    {
    public:
        void registerPanel(npp::ui::DockablePanelDescriptor) override {}
        void unregisterPanel(const npp::ui::PanelId&) override {}
        void setLayout(const npp::ui::DockLayoutSnapshot&) override {}
        npp::ui::DockLayoutSnapshot captureLayout() const override
        {
            return {};
        }
        void showPanel(const npp::ui::PanelId&) override {}
        void hidePanel(const npp::ui::PanelId&) override {}
        bool isPanelVisible(const npp::ui::PanelId&) const override
        {
            return false;
        }
    };

    class StubWindow final : public npp::ui::Window
    {
    public:
        void show() override {}
        void hide() override {}
        void requestUserAttention() override {}
        void setTitle(const npp::ui::UiString&) override {}
        npp::ui::UiString title() const override
        {
            return L"";
        }
        void setEventHandlers(npp::ui::WindowEventHandlers) override {}
        void setDockingHost(std::shared_ptr<npp::ui::DockingHost> host) override
        {
            host_ = std::move(host);
        }
        npp::ui::DockingHost* dockingHost() const noexcept override
        {
            return host_.get();
        }
        void setMinimumContentSize(std::uint32_t, std::uint32_t) override {}

    private:
        std::shared_ptr<npp::ui::DockingHost> host_{};
    };

    class StubCommand final : public npp::ui::Command
    {
    public:
        const npp::ui::CommandId& identifier() const noexcept override
        {
            return id_;
        }
        const npp::ui::UiString& title() const noexcept override
        {
            return title_;
        }
        const npp::ui::UiString& description() const noexcept override
        {
            return description_;
        }
        npp::ui::CommandState state() const noexcept override
        {
            return state_;
        }
        void setState(const npp::ui::CommandState& newState) override
        {
            state_ = newState;
        }
        void trigger() override
        {
            ++triggerCount_;
        }

        npp::ui::CommandId id_{};
        npp::ui::UiString title_{};
        npp::ui::UiString description_{};
        npp::ui::CommandState state_{};
        std::size_t triggerCount_{0};
    };

    class StubCommandRegistry final : public npp::ui::CommandRegistry
    {
    public:
        std::shared_ptr<npp::ui::Command> registerCommand(npp::ui::CommandDescriptor descriptor) override
        {
            auto command = std::make_shared<StubCommand>();
            command->id_ = descriptor.identifier;
            command->title_ = descriptor.title;
            command->description_ = descriptor.description;
            command->state_ = {};
            if (descriptor.action)
            {
                actions_[descriptor.identifier] = descriptor.action;
            }
            commands_[descriptor.identifier] = command;
            return command;
        }

        std::shared_ptr<npp::ui::Command> find(const npp::ui::CommandId& identifier) const noexcept override
        {
            const auto it = commands_.find(identifier);
            return it == commands_.end() ? nullptr : it->second;
        }

        void unregister(const npp::ui::CommandId& identifier) override
        {
            commands_.erase(identifier);
            actions_.erase(identifier);
        }

        void clear() override
        {
            commands_.clear();
            actions_.clear();
        }

        void trigger(const npp::ui::CommandId& identifier)
        {
            const auto itAction = actions_.find(identifier);
            if (itAction != actions_.end())
            {
                itAction->second();
            }

            const auto itCommand = commands_.find(identifier);
            if (itCommand != commands_.end())
            {
                std::static_pointer_cast<StubCommand>(itCommand->second)->trigger();
            }
        }

    private:
        std::unordered_map<npp::ui::CommandId, std::shared_ptr<npp::ui::Command>> commands_{};
        std::unordered_map<npp::ui::CommandId, std::function<void()>> actions_{};
    };

    class StubShortcutMap final : public npp::ui::ShortcutMap
    {
    public:
        void bind(const npp::ui::CommandId& identifier, npp::ui::Shortcut shortcut) override
        {
            bindings_[identifier] = std::move(shortcut);
        }

        std::optional<npp::ui::Shortcut> lookup(const npp::ui::CommandId& identifier) const override
        {
            const auto it = bindings_.find(identifier);
            if (it == bindings_.end())
            {
                return std::nullopt;
            }
            return it->second;
        }

        void remove(const npp::ui::CommandId& identifier) override
        {
            bindings_.erase(identifier);
        }

        void clear() override
        {
            bindings_.clear();
        }

        void importWindowsBinding(const npp::ui::CommandId& identifier, const std::wstring& windowsDefinition) override
        {
            npp::ui::Shortcut shortcut;
            shortcut.chords.push_back({npp::ui::KeyModifier::Control, std::string(windowsDefinition.begin(), windowsDefinition.end())});
            bindings_[identifier] = std::move(shortcut);
        }

    private:
        std::unordered_map<npp::ui::CommandId, npp::ui::Shortcut> bindings_{};
    };

    class StubThemeProvider final : public npp::ui::ThemeProvider
    {
    public:
        npp::ui::ThemePalette palette() const override
        {
            return palette_;
        }

        void addObserver(npp::ui::ThemeObserver& observer) override
        {
            observers_.push_back(&observer);
        }

        void removeObserver(npp::ui::ThemeObserver& observer) override
        {
            observers_.erase(std::remove(observers_.begin(), observers_.end(), &observer), observers_.end());
        }

        void publish(const npp::ui::ThemePalette& palette)
        {
            palette_ = palette;
            for (auto* observer : observers_)
            {
                observer->onThemeChanged(palette_);
            }
        }

    private:
        npp::ui::ThemePalette palette_{};
        std::vector<npp::ui::ThemeObserver*> observers_{};
    };

    class StubMenuModel final : public npp::ui::MenuModel
    {
    public:
        void setItems(std::vector<npp::ui::MenuItemDescriptor> items) override
        {
            items_ = std::move(items);
        }

        void updateState(const npp::ui::MenuId& identifier, bool enabled, bool checked) override
        {
            lastUpdated_ = identifier;
            lastEnabled_ = enabled;
            lastChecked_ = checked;
        }

        void setApplicationMenu(const npp::ui::MenuId& identifier) override
        {
            applicationMenu_ = identifier;
        }

        void rebuild() override
        {
            ++rebuildCount_;
        }

        std::vector<npp::ui::MenuItemDescriptor> items_{};
        npp::ui::MenuId applicationMenu_{};
        npp::ui::MenuId lastUpdated_{};
        bool lastEnabled_{true};
        bool lastChecked_{false};
        std::size_t rebuildCount_{0};
    };

    class StubApplication final : public npp::ui::Application
    {
    public:
        StubApplication()
            : window_{std::make_unique<StubWindow>()}
        {
        }

        npp::ui::Window& mainWindow() noexcept override
        {
            return *window_;
        }

        npp::ui::CommandRegistry& commands() noexcept override
        {
            return commandRegistry_;
        }

        npp::ui::MenuModel& menuModel() noexcept override
        {
            return menuModel_;
        }

        npp::ui::ShortcutMap& shortcuts() noexcept override
        {
            return shortcutMap_;
        }

        npp::ui::ThemeProvider& theme() noexcept override
        {
            return themeProvider_;
        }

        void integrateDockingHost(std::shared_ptr<npp::ui::DockingHost> host) override
        {
            window_->setDockingHost(std::move(host));
        }

        void dispatchPendingEvents() override
        {
        }

        int run() override
        {
            ++runCount_;
            return 0;
        }

        std::size_t runCount() const noexcept
        {
            return runCount_;
        }

    private:
        std::unique_ptr<StubWindow> window_;
        StubCommandRegistry commandRegistry_{};
        StubMenuModel menuModel_{};
        StubShortcutMap shortcutMap_{};
        StubThemeProvider themeProvider_{};
        std::size_t runCount_{0};
    };
}

TEST_CASE("Factory warns when backend is missing")
{
    npp::ui::Factory::resetBackend();

    const npp::ui::ApplicationConfig config{
        L"Notepad++",
        L"Don Ho",
        {L"/tmp", L"/tmp/translations", L"/tmp/work"},
        {},
        npp::ui::ActivationPolicy::Regular};

    npp::ui::BootstrapResult result = npp::ui::Factory::create(config);

    REQUIRE_FALSE(result.success());
    REQUIRE(result.warnings.size() == 1);
}

TEST_CASE("Factory returns instance from registered backend")
{
    npp::ui::Factory::registerBackend([](const npp::ui::ApplicationConfig&) {
        npp::ui::BootstrapResult bootstrap;
        bootstrap.application = std::make_unique<StubApplication>();
        return bootstrap;
    });

    const npp::ui::ApplicationConfig config{
        L"Notepad++",
        L"Don Ho",
        {L"/tmp", L"/tmp/translations", L"/tmp/work"},
        {},
        npp::ui::ActivationPolicy::Regular};

    npp::ui::BootstrapResult result = npp::ui::Factory::create(config);
    REQUIRE(result.success());
    REQUIRE(result.warnings.empty());

    auto& window = result.application->mainWindow();
    window.show();
    REQUIRE(window.dockingHost() == nullptr);

    auto dockingHost = std::make_shared<StubDockingHost>();
    result.application->integrateDockingHost(dockingHost);
    REQUIRE(window.dockingHost() == dockingHost.get());

    REQUIRE(result.application->run() == 0);

    npp::ui::Factory::resetBackend();
}
