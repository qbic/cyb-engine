#pragma once
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include "core/platform.h"

namespace cyb
{
    enum class FileChangeAction
    {
        Invalid,
        Added,
        Removed,
        Modified,
        RenamedNewName,
        RenamedOldName
    };

    struct FileChangeEvent
    {
        std::string filename;       // relative to watched dir
        FileChangeAction action;
        uint64_t fileSize;
        uint64_t lastWriteTime;
    };

    namespace detail
    {
        // when gimp/photoshop etc writes a file it can generate many
        // file modified events. this class with help filtering out all
        // events during an asset write.
        class StableFileEventQueue
        {
        public:
            using Clock = std::chrono::steady_clock;

            void Enqueue(const FileChangeEvent& event);
            std::vector<FileChangeEvent> PollStableFiles(int delayMs = 200);

        private:
            struct Entry
            {
                FileChangeEvent event;
                Clock::time_point time;
            };

            std::unordered_map<std::string, Entry> m_files;
            std::mutex m_mutex;
        };
    }

    class DirectoryWatcher
    {
    public:
        using Callback = std::function<void(const FileChangeEvent& event)>;

        DirectoryWatcher() = default;
        ~DirectoryWatcher();

        void Start();
        void Stop();
        bool AddDirectory(const std::string& directoryPath, Callback callback, bool recursive);
        bool IsRunning() const { return m_isRunning.load(); }

    private:
#ifdef _WIN32
        struct WatchInfo
        {
            std::string directory;
            HANDLE handle = INVALID_HANDLE_VALUE;
            OVERLAPPED overlapped = {};
            HANDLE event = nullptr;
            std::vector<char> buffer;
            Callback callback;
            bool recursive = false;
            bool pending = false;
        };

        void ParseEvents(WatchInfo& info, DWORD bytes);

        std::vector<WatchInfo> m_watchInfos;
        std::thread m_watchThread;
        std::atomic_bool m_isRunning = false;
        detail::StableFileEventQueue m_stableQueue;
#endif
    };
}
