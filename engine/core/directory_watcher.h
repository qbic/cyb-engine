#pragma once
#include <functional>
#include <thread>
#include <chrono>
#include "core/mutex.h"
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
        FileChangeAction action = FileChangeAction::Invalid;
        uint64_t fileSize = 0;
        uint64_t lastWriteTime = 0;
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
            [[nodiscard]] std::vector<FileChangeEvent> PollStableFiles(int delayMs);

        private:
            struct Entry
            {
                FileChangeEvent event;
                Clock::time_point time;
            };

            std::unordered_map<std::string, Entry> m_files;
            Mutex m_mutex;
        };
    }

    [[nodiscard]] const char* FileChangeActionToStr(const FileChangeAction action);

    class DirectoryWatcher
    {
    public:
        using Callback = std::function<void(const FileChangeEvent& event)>;

        DirectoryWatcher() = default;
        ~DirectoryWatcher();
        
        void SetEnqueueToStableDelay(uint32_t delay);

        void Start();
        void Stop();
        bool AddDirectory(const std::string& directoryPath, Callback callback, bool recursive);
        [[nodiscard]] bool IsRunning() const { return m_isRunning.load(); }

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
        uint32_t m_enqueueToStableDelay = 200;        // in ms
#endif
    };
}
