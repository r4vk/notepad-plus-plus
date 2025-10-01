#pragma once

#include "UI/Docking.h"

#include <optional>
#include <string>
#include <vector>

namespace npp::ui
{
    namespace docking
    {
        struct PanelState
        {
            std::wstring name;
            int internalId{0};
            int containerId{0};
            bool floating{false};
            bool visible{true};
            int order{0};
        };

        struct ContainerState
        {
            int identifier{0};
            int activeTab{0};
        };

        struct FloatingState
        {
            int containerId{0};
            long left{0};
            long top{0};
            long right{0};
            long bottom{0};
        };

        struct PortableLayout
        {
            int leftWidth{0};
            int rightWidth{0};
            int topHeight{0};
            int bottomHeight{0};
            std::vector<ContainerState> containers;
            std::vector<PanelState> panels;
            std::vector<FloatingState> floating;
        };

        DockLayoutSnapshot encode(const PortableLayout& layout, ThemeVariant variant);

        std::optional<PortableLayout> decode(const DockLayoutSnapshot& snapshot);
    }
}
