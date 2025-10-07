#pragma once

#include <cstddef>
#include <cstdint>

#include "NotepadPlusMsgs.h"
#include "../scintilla/Scintilla.h"

namespace npp::plugin
{
#if defined(_WIN32)
    using WindowHandle = HWND;
#else
    using WindowHandle = void*;
#endif

    struct NppData
    {
        WindowHandle _nppHandle{};
        WindowHandle _scintillaMainHandle{};
        WindowHandle _scintillaSecondHandle{};
    };

    struct ShortcutKey
    {
        bool _isCtrl{};
        bool _isAlt{};
        bool _isShift{};
        std::uint8_t _key{};
    };

    constexpr std::size_t menuItemSize = 64;

    struct FuncItem
    {
        wchar_t _itemName[menuItemSize]{};
        void (*_pFunc)() = nullptr;
        int _cmdID{};
        bool _initToCheck{};
        ShortcutKey* _pShKey = nullptr;
    };

#if defined(__APPLE__)
#    define NPP_PLUGIN_EXPORT __attribute__((visibility("default")))
#elif defined(_WIN32)
#    define NPP_PLUGIN_EXPORT __declspec(dllexport)
#else
#    define NPP_PLUGIN_EXPORT
#endif

    using MessageResult = std::intptr_t;
}

extern "C"
{
    NPP_PLUGIN_EXPORT void setInfo(npp::plugin::NppData data);
    NPP_PLUGIN_EXPORT const wchar_t* getName();
    NPP_PLUGIN_EXPORT npp::plugin::FuncItem* getFuncsArray(int* count);
    NPP_PLUGIN_EXPORT void beNotified(SCNotification* notification);
    NPP_PLUGIN_EXPORT npp::plugin::MessageResult messageProc(std::uint32_t message, std::uintptr_t wParam, std::intptr_t lParam);
    NPP_PLUGIN_EXPORT bool isUnicode();
}

