#include "Platform/FileChangePoller.h"

#include <system_error>

namespace npp::platform
{
    FileChangePoller::FileChangePoller(std::filesystem::path path, FileWatchEvent events)
        : path_(std::move(path))
        , trackSize_(hasFlag(events, FileWatchEvent::Size))
        , trackLastWrite_(hasFlag(events, FileWatchEvent::LastWrite))
    {
    }

    bool FileChangePoller::detectChanges()
    {
        std::error_code existsEc;
        const bool exists = std::filesystem::exists(path_, existsEc) && !existsEc;

        bool changed = false;

        if (exists)
        {
            if (trackSize_)
            {
                std::error_code sizeEc;
                const auto size = std::filesystem::file_size(path_, sizeEc);
                if (!sizeEc)
                {
                    if (lastSize_ && *lastSize_ != size)
                    {
                        changed = true;
                    }
                    lastSize_ = size;
                }
            }

            if (trackLastWrite_)
            {
                std::error_code timeEc;
                const auto writeTime = std::filesystem::last_write_time(path_, timeEc);
                if (!timeEc)
                {
                    if (lastWrite_ && *lastWrite_ != writeTime)
                    {
                        changed = true;
                    }
                    lastWrite_ = writeTime;
                }
            }

            if (!initialCheck_ && previouslyMissing_)
            {
                changed = true;
            }

            previouslyMissing_ = false;
        }
        else
        {
            if (!initialCheck_ && !previouslyMissing_)
            {
                changed = true;
            }

            previouslyMissing_ = true;
            lastSize_.reset();
            lastWrite_.reset();
        }

        initialCheck_ = false;
        return changed;
    }
}
