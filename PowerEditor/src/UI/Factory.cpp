#include "UI/Factory.h"

#include <mutex>
#include <utility>

namespace npp::ui
{
    namespace
    {
        std::mutex g_backendMutex;
        Factory::Backend g_backend;
    }

    void Factory::registerBackend(Backend backend)
    {
        std::lock_guard<std::mutex> lock(g_backendMutex);
        g_backend = std::move(backend);
    }

    void Factory::resetBackend()
    {
        std::lock_guard<std::mutex> lock(g_backendMutex);
        g_backend = nullptr;
    }

    BootstrapResult Factory::create(const ApplicationConfig& config)
    {
        Factory::Backend backendCopy;
        {
            std::lock_guard<std::mutex> lock(g_backendMutex);
            backendCopy = g_backend;
        }

        if (!backendCopy)
        {
            BootstrapResult result;
            result.warnings.push_back({L"No UI backend registered"});
            return result;
        }

        return backendCopy(config);
    }
}

