#include "core/directory_watcher.h"
#include "core/filesystem.h"
#include "core/logger.h"

namespace cyb
{
    namespace detail
    {
        void StableFileEventQueue::Enqueue(const FileChangeEvent& event)
        {
            std::lock_guard lock(m_mutex);
            m_files[event.filename].event = event;
            m_files[event.filename].time = Clock::now();
        }

        std::vector<FileChangeEvent> StableFileEventQueue::PollStableFiles(int delayMs)
        {
            std::vector<FileChangeEvent> ready;
            auto now = Clock::now();

            std::lock_guard lock(m_mutex);
            for (auto it = m_files.begin(); it != m_files.end(); )
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.time).count();
                if (elapsed >= delayMs)
                {
                    ready.push_back(it->second.event);
                    it = m_files.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            return ready;
        }
    }

    DirectoryWatcher::~DirectoryWatcher()
    {
        Stop();
    }

    void DirectoryWatcher::Start()
    {
        if (m_isRunning)
            return;

        m_isRunning = true;
        m_watchThread = std::thread([this] {

            while (m_isRunning)
            {
                for (auto& info : m_watchInfos)
                {
                    if (!info.pending)
                    {
                        DWORD bytesReturned = 0;
                        ZeroMemory(&info.overlapped, sizeof(OVERLAPPED));
                        info.overlapped.hEvent = info.event;

                        if (!ReadDirectoryChangesExW(
                            info.handle,
                            info.buffer.data(),
                            static_cast<DWORD>(info.buffer.size()),
                            info.recursive,
                            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME,
                            nullptr,
                            &info.overlapped,
                            nullptr,
                            ReadDirectoryNotifyExtendedInformation))
                        {
                            CYB_ERROR("DirectoryWatcher ReadDirectoryChangesW failed on: {}", info.directory);
                            continue;
                        }

                        info.pending = true;
                    }

                    DWORD wait = WaitForSingleObject(info.overlapped.hEvent, 500);
                    if (wait == WAIT_OBJECT_0)
                    {
                        DWORD bytes;
                        if (GetOverlappedResult(info.handle, &info.overlapped, &bytes, FALSE))
                            ParseEvents(info, bytes);

                        ResetEvent(info.event);
                        info.pending = false;
                    }

                    for (const auto& event : m_stableQueue.PollStableFiles())
                        info.callback(event);
                }
            }
        });
    }

    void DirectoryWatcher::Stop()
    {
        m_isRunning = false;
        if (m_watchThread.joinable())
            m_watchThread.join();
        
        for (auto& info : m_watchInfos)
        {
            if (info.handle != INVALID_HANDLE_VALUE)
                CloseHandle(info.handle);
            if (info.event)
                CloseHandle(info.event);
        }

        m_watchInfos.clear();
    }

    static FileChangeAction TranslateFileAction(DWORD action)
    {
        switch (action)
        {
        case FILE_ACTION_ADDED:
            return FileChangeAction::Added;
        case FILE_ACTION_REMOVED:
            return FileChangeAction::Removed;
        case FILE_ACTION_MODIFIED:
            return FileChangeAction::Modified;
        case FILE_ACTION_RENAMED_NEW_NAME:
            return FileChangeAction::RenamedNewName;
        case FILE_ACTION_RENAMED_OLD_NAME:
            return FileChangeAction::RenamedOldName;
        }

        return FileChangeAction::Invalid;
    }

    const char* FileChangeActionToStr(const FileChangeAction action)
    {
        switch (action)
        {
        case FileChangeAction::Added:
            return "Added";
        case FileChangeAction::Removed:
            return "Removed";
        case FileChangeAction::Modified:
            return "Modified";
        case FileChangeAction::RenamedNewName:
            return "RenamedNewName";
        case FileChangeAction::RenamedOldName:
            return "RenamedOldName";
        }

        return "Invalid";
    }

    void DirectoryWatcher::ParseEvents(WatchInfo& info, DWORD bytes)
    {
        char* base = info.buffer.data();
        size_t offset = 0;

        while (offset < bytes)
        {
            auto* fni = reinterpret_cast<FILE_NOTIFY_EXTENDED_INFORMATION*>(base + offset);

            std::wstring wideName(fni->FileName, fni->FileNameLength / sizeof(WCHAR));
            std::filesystem::path relativePath = std::filesystem::path(wideName);
            std::string filePath = relativePath.string();

            FileChangeEvent event = {};
            event.filename = filesystem::FixFilePath(filePath);
            event.action = TranslateFileAction(fni->Action);
            event.fileSize = fni->FileSize.QuadPart;
            event.lastWriteTime = fni->LastModificationTime.QuadPart;

            m_stableQueue.Enqueue(event);

            if (fni->NextEntryOffset == 0)
                break;
            offset += fni->NextEntryOffset;
        }
    }

    bool DirectoryWatcher::AddDirectory(const std::string& directory, Callback callback, bool recursive)
    {
        WatchInfo info = {};

        info.handle = CreateFileA(
                directory.c_str(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                nullptr);

        if (info.handle == INVALID_HANDLE_VALUE)
        {
            CYB_WARNING("DirectoryWatcher: Failed to open directory (code {}): {} ", GetLastError(), directory);
            return false;
        }

        info.directory = directory;
        info.callback = std::move(callback);
        info.buffer.resize(4 * 1024);
        ZeroMemory(&info.overlapped, sizeof(OVERLAPPED));
        info.event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        info.recursive = recursive;

        CYB_TRACE("DirectoryWatcher adding relative path \"{}\"", directory);
        m_watchInfos.emplace_back(std::move(info));
        return true;
    }
}