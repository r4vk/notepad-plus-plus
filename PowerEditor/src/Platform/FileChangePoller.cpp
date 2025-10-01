#include "Platform/FileChangePoller.h"

#include <atomic>
#include <string>
#include <system_error>
#include <utility>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace npp::platform
{
    namespace
    {
        class FileChangePollerImpl
        {
        public:
            FileChangePollerImpl(std::filesystem::path path, FileWatchEvent events);
            ~FileChangePollerImpl();

            FileChangePollerImpl(FileChangePollerImpl&& other) noexcept = delete;
            FileChangePollerImpl& operator=(FileChangePollerImpl&& other) noexcept = delete;

            bool detectChanges();
            const std::filesystem::path& path() const noexcept { return path_; }

        private:
            bool pollFileSystem();

#if defined(__APPLE__)
            void setupWatcher();
            void cleanupWatcher();
            static void eventHandler(void* context);
#endif

            std::filesystem::path path_;
            bool trackSize_ = false;
            bool trackLastWrite_ = false;
            bool previouslyMissing_ = false;
            bool initialCheck_ = true;
            std::optional<std::uintmax_t> lastSize_;
            std::optional<std::filesystem::file_time_type> lastWrite_;

#if defined(__APPLE__)
            dispatch_queue_t queue_ = nullptr;
            dispatch_source_t source_ = nullptr;
            int descriptor_ = -1;
            std::atomic<bool> eventFlag_{false};
            std::atomic<bool> needsRearm_{false};
#endif
        };

        FileChangePollerImpl::FileChangePollerImpl(std::filesystem::path path, FileWatchEvent events)
            : path_(std::move(path))
            , trackSize_(hasFlag(events, FileWatchEvent::Size))
            , trackLastWrite_(hasFlag(events, FileWatchEvent::LastWrite))
        {
#if defined(__APPLE__)
            queue_ = dispatch_queue_create("org.notepadplusplus.filewatcher", DISPATCH_QUEUE_SERIAL);
            setupWatcher();
#endif
        }

        FileChangePollerImpl::~FileChangePollerImpl()
        {
#if defined(__APPLE__)
            cleanupWatcher();
            if (queue_)
            {
                dispatch_release(queue_);
                queue_ = nullptr;
            }
#endif
        }

        bool FileChangePollerImpl::detectChanges()
        {
#if defined(__APPLE__)
            if (needsRearm_.exchange(false, std::memory_order_acq_rel))
            {
                setupWatcher();
            }
#endif

            bool changed = false;

#if defined(__APPLE__)
            if (eventFlag_.exchange(false, std::memory_order_acq_rel))
            {
                changed = true;
            }
#endif

            if (pollFileSystem())
            {
                changed = true;
            }

#if defined(__APPLE__)
            if (!source_)
            {
                setupWatcher();
            }
#endif

            return changed;
        }

        bool FileChangePollerImpl::pollFileSystem()
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
#if defined(__APPLE__)
                cleanupWatcher();
#endif
            }

            initialCheck_ = false;
            return changed;
        }

#if defined(__APPLE__)
        void FileChangePollerImpl::setupWatcher()
        {
            cleanupWatcher();

            if (!queue_)
            {
                return;
            }

            const std::string pathString = path_.string();
            descriptor_ = open(pathString.c_str(), O_EVTONLY);
            if (descriptor_ == -1)
            {
                return;
            }

            unsigned long mask = DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_EXTEND |
                                 DISPATCH_VNODE_ATTRIB | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_REVOKE;

            source_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, static_cast<uintptr_t>(descriptor_), mask, queue_);
            if (!source_)
            {
                close(descriptor_);
                descriptor_ = -1;
                return;
            }

            dispatch_set_context(source_, this);
            dispatch_source_set_event_handler_f(source_, &FileChangePollerImpl::eventHandler);
            dispatch_source_set_cancel_handler_f(source_, &FileChangePollerImpl::cancelHandler);
            dispatch_resume(source_);
        }

        void FileChangePollerImpl::cleanupWatcher()
        {
            if (source_)
            {
                dispatch_source_cancel(source_);
                dispatch_release(source_);
                source_ = nullptr;
            }

            if (descriptor_ != -1)
            {
                close(descriptor_);
                descriptor_ = -1;
            }
        }

        void FileChangePollerImpl::handleEvent()
        {
            eventFlag_.store(true, std::memory_order_release);
            if (!source_)
            {
                return;
            }
            const unsigned long data = dispatch_source_get_data(source_);
            if (data & (DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_REVOKE))
            {
                needsRearm_.store(true, std::memory_order_release);
            }
        }

        void FileChangePollerImpl::handleCancel()
        {
            if (descriptor_ != -1)
            {
                close(descriptor_);
                descriptor_ = -1;
            }
        }

        void FileChangePollerImpl::eventHandler(void* context)
        {
            auto* self = static_cast<FileChangePollerImpl*>(context);
            if (self)
            {
                self->handleEvent();
            }
        }
#endif
    }

    FileChangePoller::FileChangePoller(std::filesystem::path path, FileWatchEvent events)
        : impl_(std::make_unique<FileChangePollerImpl>(std::move(path), events))
    {
    }

    FileChangePoller::~FileChangePoller() = default;

    FileChangePoller::FileChangePoller(FileChangePoller&&) noexcept = default;

    FileChangePoller& FileChangePoller::operator=(FileChangePoller&&) noexcept = default;

    bool FileChangePoller::detectChanges()
    {
        return impl_->detectChanges();
    }

    const std::filesystem::path& FileChangePoller::path() const noexcept
    {
        return impl_->path();
    }
}
