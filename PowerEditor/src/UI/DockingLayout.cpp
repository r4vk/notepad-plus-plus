#include "UI/DockingLayout.h"

#include <algorithm>
#include <codecvt>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace
{
    std::string toUtf8(const std::wstring& value)
    {
        static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(value);
    }

    std::wstring fromUtf8(const std::string& value)
    {
        static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(value);
    }

    void consumeLine(std::istringstream& stream)
    {
        stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

namespace npp::ui
{
    namespace docking
    {
        DockLayoutSnapshot encode(const PortableLayout& layout, ThemeVariant variant)
        {
            std::ostringstream stream;
            stream << "docklayout-v1" << '\n';
            stream << layout.leftWidth << ' ' << layout.rightWidth << ' '
                   << layout.topHeight << ' ' << layout.bottomHeight << '\n';

            stream << layout.containers.size() << '\n';
            for (const auto& container : layout.containers)
            {
                stream << container.identifier << ' ' << container.activeTab << '\n';
            }

            stream << layout.panels.size() << '\n';
            for (const auto& panel : layout.panels)
            {
                stream << panel.internalId << ' ' << panel.containerId << ' '
                       << (panel.floating ? 1 : 0) << ' ' << (panel.visible ? 1 : 0)
                       << ' ' << panel.order << ' '
                       << std::quoted(toUtf8(panel.name)) << '\n';
            }

            stream << layout.floating.size() << '\n';
            for (const auto& floating : layout.floating)
            {
                stream << floating.containerId << ' ' << floating.left << ' ' << floating.top
                       << ' ' << floating.right << ' ' << floating.bottom << '\n';
            }

            DockLayoutSnapshot snapshot;
            snapshot.encodedState = stream.str();
            snapshot.description = L"Dock layout snapshot";
            snapshot.variant = variant;
            return snapshot;
        }

        std::optional<PortableLayout> decode(const DockLayoutSnapshot& snapshot)
        {
            std::istringstream stream(snapshot.encodedState);
            std::string header;
            if (!std::getline(stream, header))
            {
                return std::nullopt;
            }
            if (header != "docklayout-v1")
            {
                return std::nullopt;
            }

            PortableLayout layout;
            if (!(stream >> layout.leftWidth >> layout.rightWidth >> layout.topHeight >> layout.bottomHeight))
            {
                return std::nullopt;
            }
            consumeLine(stream);

            std::size_t containerCount = 0;
            if (!(stream >> containerCount))
            {
                return std::nullopt;
            }
            consumeLine(stream);

            layout.containers.reserve(containerCount);
            for (std::size_t i = 0; i < containerCount; ++i)
            {
                ContainerState container{};
                if (!(stream >> container.identifier >> container.activeTab))
                {
                    return std::nullopt;
                }
                consumeLine(stream);
                layout.containers.push_back(container);
            }

            std::size_t panelCount = 0;
            if (!(stream >> panelCount))
            {
                return std::nullopt;
            }
            consumeLine(stream);

            layout.panels.reserve(panelCount);
            for (std::size_t i = 0; i < panelCount; ++i)
            {
                PanelState panel{};
                int floatingFlag = 0;
                int visibleFlag = 0;
                if (!(stream >> panel.internalId >> panel.containerId >> floatingFlag >> visibleFlag >> panel.order))
                {
                    return std::nullopt;
                }
                std::string nameUtf8;
                if (!(stream >> std::quoted(nameUtf8)))
                {
                    return std::nullopt;
                }
                consumeLine(stream);
                panel.floating = floatingFlag != 0;
                panel.visible = visibleFlag != 0;
                panel.name = fromUtf8(nameUtf8);
                layout.panels.push_back(std::move(panel));
            }

            std::size_t floatingCount = 0;
            if (!(stream >> floatingCount))
            {
                return std::nullopt;
            }
            consumeLine(stream);

            layout.floating.reserve(floatingCount);
            for (std::size_t i = 0; i < floatingCount; ++i)
            {
                FloatingState floating{};
                if (!(stream >> floating.containerId >> floating.left >> floating.top >> floating.right >> floating.bottom))
                {
                    return std::nullopt;
                }
                consumeLine(stream);
                layout.floating.push_back(floating);
            }

            return layout;
        }

    }
}
