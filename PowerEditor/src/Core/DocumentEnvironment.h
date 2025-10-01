#pragma once

#include <cstddef>
#include <cstdint>

#include "Scintilla.h"

class Buffer;

namespace npp::core
{
    using NativeWindowHandle = void*;

    class EditView
    {
    public:
        virtual ~EditView() = default;

        virtual bool isWrapEnabled() const = 0;
        virtual std::size_t documentLength() const = 0;
        virtual sptr_t sendEditorMessage(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0) = 0;
        virtual NativeWindowHandle nativeHandle() const = 0;
    };

    class DocumentEnvironment
    {
    public:
        virtual ~DocumentEnvironment() = default;

        virtual Buffer* currentBuffer() const = 0;
        virtual EditView& editView() = 0;
        virtual const EditView& editView() const = 0;
        virtual void notifyBufferChanged(Buffer* buffer, int mask) = 0;
        virtual void executeCommand(int commandId) = 0;
        virtual void saveCurrentSession() = 0;
        virtual void loadBufferIntoView(Buffer* buffer, int whichView) = 0;
    };
}
