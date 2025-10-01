#include "catch.hpp"

#include "UI/DockingLayout.h"

TEST_CASE("Docking layout encode decode roundtrip")
{
    npp::ui::docking::PortableLayout layout;
    layout.leftWidth = 240;
    layout.rightWidth = 360;
    layout.topHeight = 180;
    layout.bottomHeight = 420;

    layout.containers.push_back({0, 2});
    layout.containers.push_back({1, 0});

    npp::ui::docking::PanelState functionList;
    functionList.name = L"Function List";
    functionList.internalId = 1;
    functionList.containerId = 0;
    functionList.visible = true;
    layout.panels.push_back(functionList);

    npp::ui::docking::PanelState documentMap;
    documentMap.name = L"Document Map";
    documentMap.internalId = 2;
    documentMap.containerId = 4;
    documentMap.floating = true;
    documentMap.visible = false;
    layout.panels.push_back(documentMap);

    npp::ui::docking::FloatingState floating;
    floating.containerId = 4;
    floating.left = 100;
    floating.top = 120;
    floating.right = 420;
    floating.bottom = 480;
    layout.floating.push_back(floating);

    const auto snapshot = npp::ui::docking::encode(layout, npp::ui::ThemeVariant::Light);
    const auto decoded = npp::ui::docking::decode(snapshot);

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->leftWidth == layout.leftWidth);
    REQUIRE(decoded->panels.size() == layout.panels.size());
    REQUIRE(decoded->panels[0].name == layout.panels[0].name);
    REQUIRE(decoded->panels[1].floating);
    REQUIRE(decoded->floating.size() == 1);
    REQUIRE(decoded->floating[0].right == floating.right);
}

