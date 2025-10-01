#pragma once

#include "UI/Command.h"
#include "UI/Menu.h"
#include "UI/Shortcut.h"
#include "UI/Theme.h"
#include "UI/Window.h"

#include <memory>
#include <optional>
#include <vector>

namespace npp::ui
{
    class Application;

    struct ApplicationPaths
    {
        std::wstring resources;
        std::wstring translations;
        std::wstring workspace;
    };

    struct ApplicationSecurityPolicy
    {
        bool enableCrashReporting{true};
        bool enablePluginSandbox{true};
    };

    struct ApplicationConfig
    {
        UiString applicationName;
        UiString organizationName;
        ApplicationPaths paths;
        ApplicationSecurityPolicy securityPolicy{};
        ActivationPolicy activationPolicy{ActivationPolicy::Regular};
    };

    struct BootstrapWarning
    {
        UiString message;
    };

    struct BootstrapResult
    {
        std::unique_ptr<Application> application;
        std::vector<BootstrapWarning> warnings;

        [[nodiscard]] bool success() const noexcept
        {
            return static_cast<bool>(application);
        }
    };

    class Application
    {
    public:
        virtual ~Application() = default;

        virtual Window& mainWindow() noexcept = 0;
        virtual CommandRegistry& commands() noexcept = 0;
        virtual MenuModel& menuModel() noexcept = 0;
        virtual ShortcutMap& shortcuts() noexcept = 0;
        virtual ThemeProvider& theme() noexcept = 0;
        virtual void integrateDockingHost(std::shared_ptr<DockingHost> host) = 0;
        virtual void dispatchPendingEvents() = 0;
        virtual int run() = 0;
    };
}
