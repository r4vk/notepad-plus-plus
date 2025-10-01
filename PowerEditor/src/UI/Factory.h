#pragma once

#include "UI/Application.h"

#include <functional>

namespace npp::ui
{
    class Factory
    {
    public:
        using Backend = std::function<BootstrapResult(const ApplicationConfig& config)>;

        static void registerBackend(Backend backend);
        static void resetBackend();
        static BootstrapResult create(const ApplicationConfig& config);
    };
}

