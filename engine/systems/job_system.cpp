#include <thread>
#include <deque>
#include <condition_variable>
#if defined(_WIN32)
#include <Windows.h>
#endif // _WIN32
#include "core/platform.h"
#include "core/logger.h"
#include "systems/job_system.h"

namespace cyb::jobsystem
{
    struct Job
    {
        std::function<void(JobArgs)> task;
        Context* ctx = nullptr;
        uint32_t groupID = 0;
        uint32_t groupJobOffset = 0;
        uint32_t groupJobEnd = 0;
        uint32_t sharedMemorySize = 0;

        void Execute()
        {
            JobArgs args = {};
            args.groupID = groupID;
            args.sharedMemory = nullptr;

            if (sharedMemorySize > 0)
                args.sharedMemory = _malloca(sharedMemorySize);

            for (uint32_t i = groupJobOffset; i < groupJobEnd; ++i)
            {
                args.jobIndex = i;
                args.groupIndex = i - groupJobOffset;
                args.isFirstJobInGroup = (i == groupJobOffset);
                args.isLastJobInGroup = (i == groupJobEnd - 1);
                task(args);
            }

            // if the allocation was made on the heap we need to free it
            if (args.sharedMemory != nullptr)
                _freea(args.sharedMemory);

            ctx->counter.fetch_sub(1);
        }
    };

    struct JobQueue
    {
        std::deque<Job> queue;
        std::mutex locker;

        inline void PushBack(const Job& item)
        {
            std::scoped_lock lock(locker);
            queue.push_back(item);
        }

        inline bool PopFront(Job& item)
        {
            std::scoped_lock lock(locker);
            if (queue.empty())
                return false;
            
            item = std::move(queue.front());
            queue.pop_front();
            return true;
        }
    };

    // This structure is responsible to stop worker thread loops.
    // Once this is destroyed, worker threads will be woken up and end their loops.
    struct InternalState
    {
        uint32_t numCores = 0;
        uint32_t numThreads = 0;
        std::unique_ptr<JobQueue[]> jobQueuePerThread;
        std::atomic_bool alive{ true };
        std::condition_variable wakeCondition;
        std::mutex wakeMutex;
        std::atomic<uint32_t> nextQueue{ 0 };
        std::vector<std::thread> threads;

        ~InternalState()
        {
            alive.store(false); // indicate that new jobs cannot be started from this point
            bool wake_loop = true;
            std::thread waker([&] {
                // wake up sleeping worker threads
                while (wake_loop)
                    wakeCondition.notify_all();
            });

            for (auto& thread : threads)
                thread.join();

            wake_loop = false;
            waker.join();
        }
    };

    static InternalState internal_state;

    // Start working on a job queue
    // After the job queue is finished, it can switch to an other queue and steal jobs from there
    inline void work(uint32_t startingQueue)
    {
        Job job;
        for (uint32_t i = 0; i < internal_state.numThreads; ++i)
        {
            JobQueue& job_queue = internal_state.jobQueuePerThread[startingQueue % internal_state.numThreads];
            while (job_queue.PopFront(job))
                job.Execute();

            startingQueue++;
        }
    }

    void Initialize()
    {
        assert(internal_state.numThreads == 0 && "allready initialized");

        // retrieve the number of hardware threads in this system
        internal_state.numCores = std::thread::hardware_concurrency();

        // calculate the actual number of worker threads we want
        internal_state.numThreads = std::max(1u, internal_state.numCores - 1); // -1 for main thread
        internal_state.jobQueuePerThread = std::make_unique<JobQueue[]>(internal_state.numThreads);
        internal_state.threads.reserve(internal_state.numThreads);

        // start from 1, leaving the main thread free
        for (uint32_t threadID = 0; threadID < internal_state.numThreads; ++threadID)
        {
            std::thread& worker = internal_state.threads.emplace_back([threadID] {
                while (internal_state.alive.load())
                {
                    work(threadID);

                    // finished with jobs, put to sleep
                    std::unique_lock<std::mutex> lock(internal_state.wakeMutex);
                    internal_state.wakeCondition.wait(lock);
                }
            });

#if defined(_WIN32)
            HANDLE handle = (HANDLE)worker.native_handle();
            std::wstring wthreadname = L"cyb::thread_" + std::to_wstring(threadID);
            HRESULT hr = SetThreadDescription(handle, wthreadname.c_str());
            assert(SUCCEEDED(hr));
#endif // _WIN32
        }

        CYB_INFO("JobSystem Initialized with [{0} cores] [{1} threads]", internal_state.numCores, internal_state.numThreads);
    }

    uint32_t GetThreadCount()
    {
        return internal_state.numThreads;
    }

    void Execute(Context& ctx, const std::function<void(JobArgs)>& task)
    {
        // context state is updated
        ctx.counter.fetch_add(1);

        Job job;
        job.ctx = &ctx;
        job.task = task;
        job.groupID = 0;
        job.groupJobOffset = 0;
        job.groupJobEnd = 1;
        job.sharedMemorySize = 0;

        internal_state.jobQueuePerThread[internal_state.nextQueue.fetch_add(1) % internal_state.numThreads].PushBack(job);
        internal_state.wakeCondition.notify_one();
    }

    uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize)
    {
        // Calculate the amount of job groups to dispatch (overestimate, or "ceil")
        return (jobCount + groupSize - 1) / groupSize;
    }

    void Dispatch(Context& ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)>& task)
    {
        if (jobCount == 0 || groupSize == 0)
            return;

        const uint32_t groupCount = DispatchGroupCount(jobCount, groupSize);

        // Context state is updated
        ctx.counter.fetch_add(groupCount);

        Job job;
        job.ctx = &ctx;
        job.task = task;

        for (uint32_t groupID = 0; groupID < groupCount; ++groupID)
        {
            // For each group, generate one real job
            job.groupID = groupID;
            job.groupJobOffset = groupID * groupSize;
            job.groupJobEnd = std::min(job.groupJobOffset + groupSize, jobCount);

            internal_state.jobQueuePerThread[internal_state.nextQueue.fetch_add(1) % internal_state.numThreads].PushBack(job);
        }

        internal_state.wakeCondition.notify_all();
    }

    bool IsBusy(const Context& ctx)
    {
        // whenever the context label is greater than zero, it means that there is
        // still work that needs to be done
        return ctx.counter.load() > 0;
    }

    void Wait(const Context& ctx)
    {
        if (IsBusy(ctx))
        {
            // Wake any threads that might be sleeping
            internal_state.wakeCondition.notify_all();

            // work() will pick up any jobs that are on stand by and execute them on this thread
            work(internal_state.nextQueue.fetch_add(1) % internal_state.numThreads);

            while (IsBusy(ctx))
            {
                // If we are here, then there are still remaining jobs that work() couldn't pick up.
                //	In this case those jobs are not standing by on a queue but currently executing
                //	on other threads, so they cannot be picked up by this thread.
                //	Allow to swap out this thread by OS to not spin endlessly for nothing
                std::this_thread::yield();
            }
        }
    }
}
