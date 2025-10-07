#include "npp/PluginAPI.h"

#include <array>
#include <cwchar>

using namespace npp::plugin;

namespace
{
    NppData gNppData{};
    ShortcutKey gAboutShortcut{};
    std::array<FuncItem, 1> gFuncItems{};

    void aboutCommand()
    {
        // On macOS the UI bridge will surface this message in a native alert.
        // For the sample we only demonstrate how to send a notification via messageProc.
        const wchar_t* message = L"Hello from the macOS Notepad++ Plugin SDK";
        // NPPM_SETSTATUSBAR is supported on all platforms and routes to the status UI.
        ::messageProc(NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, reinterpret_cast<std::intptr_t>(message));
    }
}

extern "C"
{
    NPP_PLUGIN_EXPORT void setInfo(NppData data)
    {
        gNppData = data;

        FuncItem& item = gFuncItems.front();
        std::wcsncpy(item._itemName, L"About macOS SDK", menuItemSize - 1);
        item._itemName[menuItemSize - 1] = L'\0';
        item._pFunc = &aboutCommand;
        item._initToCheck = false;
        gAboutShortcut._isCtrl = false;
        gAboutShortcut._isAlt = false;
        gAboutShortcut._isShift = true;
        gAboutShortcut._key = 'A';
        item._pShKey = &gAboutShortcut;
    }

    NPP_PLUGIN_EXPORT const wchar_t* getName()
    {
        return L"HelloMacPlugin";
    }

    NPP_PLUGIN_EXPORT FuncItem* getFuncsArray(int* count)
    {
        if (count)
        {
            *count = static_cast<int>(gFuncItems.size());
        }

        return gFuncItems.data();
    }

    NPP_PLUGIN_EXPORT void beNotified(SCNotification* notification)
    {
        (void)notification;
    }

    NPP_PLUGIN_EXPORT MessageResult messageProc(std::uint32_t message, std::uintptr_t wParam, std::intptr_t lParam)
    {
        (void)message;
        (void)wParam;
        (void)lParam;
        return 0;
    }

    NPP_PLUGIN_EXPORT bool isUnicode()
    {
        return true;
    }
}
