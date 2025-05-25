#include <thread>
#include <semaphore>
#include <deque>
#include <condition_variable>
#if defined(_WIN32)
#include <Windows.h>
#endif // _WIN32
#include "core/atomic_queue.h"
#include "core/platform.h"
#include "core/logger.h"
#include "core/non_copyable.h"
#include "systems/job_system.h"

namespace cyb::jobsystem
{
    struct Job : private MovableNonCopyable
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

            ctx->counter.fetch_sub(1, std::memory_order_acq_rel);
        }
    };

    using AtomicJobQueue = AtomicQueue<Job, 1024>;

    // This structure is responsible to stop worker thread loops.
    // Once this is destroyed, worker threads will be woken up and end their loops.
    struct InternalState
    {
        uint32_t numCores = 0;
        uint32_t numThreads = 0;
        std::thread::id mainThreadId;
        std::unique_ptr<AtomicJobQueue[]> jobQueuePerThread;
        std::atomic_bool alive{ true };
        std::counting_semaphore<> wakeSemaphore{ 0 };
        std::atomic<uint32_t> alignas(64) nextQueue{ 0 };
        std::vector<std::thread> threads;

        void Submit(Job&& job)
        {
            auto& queue = jobQueuePerThread[nextQueue.fetch_add(1) % numThreads];
            queue.Push(std::move(job));
            wakeSemaphore.release(1);
        }

        ~InternalState()
        {
            // indicate that new jobs cannot be started from this point
            alive.store(false);

            bool wake_loop = true;
            std::thread waker([&] {
                // wake up sleeping worker threads
                while (wake_loop)
                    wakeSemaphore.release(numThreads);
            });

            for (auto& thread : threads)
                thread.join();

            wake_loop = false;
            waker.join();
        }
    };

    static InternalState internal_state;
    thread_local uint32_t localQueueIndex = 0;

    // Start working on a job queue.
    // After the job queue is finished, it can switch to an other queue
    // and steal jobs from there.
    static void Work()
    {
        Job job;
        AtomicJobQueue& jobQueue = internal_state.jobQueuePerThread[localQueueIndex];
        while (true)
        {
            // try to pop from local queue
            if (jobQueue.Pop(job))
            {
                bool isWorkerThread = (std::this_thread::get_id() != internal_state.mainThreadId);
                if (job.ctx->allowWorkOnMainThread || isWorkerThread)
                {
                    job.Execute();
                }
                else
                {
                    // skip executing this job and push it back to the queue
                    jobQueue.Push(std::move(job));
                    break;
                }
            }
            else
            {
                // try to steal from other queues
                // start from the next queue, to avoid contention with the current queue
                bool stolen = false;
                uint32_t start = (localQueueIndex + 1) % internal_state.numThreads;
                for (uint32_t offset = 0; offset < internal_state.numThreads - 1; ++offset)
                {
                    uint32_t i = (start + offset) % internal_state.numThreads;
                    if (internal_state.jobQueuePerThread[i].Pop(job))
                    {
                        job.Execute();
                        stolen = true;
                        break;
                    }
                }
                if (!stolen)
                    break; // no work found, exit
            }
        }

        localQueueIndex = (localQueueIndex + 1) % internal_state.numThreads;
    }

    void Initialize()
    {
        assert(internal_state.numThreads == 0 && "allready initialized");

        // retrieve the number of hardware threads in this system
        internal_state.numCores = std::thread::hardware_concurrency();

        // calculate the actual number of worker threads we want
        internal_state.numThreads = std::max(1u, internal_state.numCores - 1);
        internal_state.jobQueuePerThread = std::make_unique<AtomicJobQueue[]>(internal_state.numThreads);
        internal_state.threads.reserve(internal_state.numThreads);
        internal_state.mainThreadId = std::this_thread::get_id();

        // explicitly set localQueueIndex for the main thread
        localQueueIndex = 0;

        for (uint32_t threadID = 0; threadID < internal_state.numThreads; ++threadID)
        {
            std::thread& worker = internal_state.threads.emplace_back([threadID] {
                localQueueIndex = threadID;
                while (internal_state.alive.load())
                {
                    Work();
                    internal_state.wakeSemaphore.acquire();
                }
            });

#if defined(_WIN32)
            HANDLE handle = (HANDLE)worker.native_handle();
            std::wstring wthreadname = L"cyb::thread_" + std::to_wstring(threadID);
            HRESULT hr = SetThreadDescription(handle, wthreadname.c_str());
            assert(SUCCEEDED(hr));

            // set thread affinity, each thread to a specific core starting from
            // second core (core 0 is the main thread)
            const int core = threadID + 1;
            const DWORD_PTR affinityMask = 1ull << core;
            SetThreadAffinityMask(handle, affinityMask);
#endif // _WIN32
        }

        CYB_INFO("JobSystem Initialized with [{} cores] [{} threads]", internal_state.numCores, internal_state.numThreads);
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

        internal_state.Submit(std::move(job));
    }

    [[nodiscard]] uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize)
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

        for (uint32_t groupID = 0; groupID < groupCount; ++groupID)
        {
            // For each group, generate one real job
            Job job;
            job.ctx = &ctx;
            job.task = task;
            job.groupID = groupID;
            job.groupJobOffset = groupID * groupSize;
            job.groupJobEnd = std::min(job.groupJobOffset + groupSize, jobCount);

            internal_state.Submit(std::move(job));
        }
    }

    bool IsBusy(const Context& ctx)
    {
        // whenever the context label is greater than zero, it means that there is
        // still work that needs to be done
        return ctx.counter.load(std::memory_order_acquire) > 0;
    }

    void Wait(const Context& ctx)
    {
        int spin = 0;
        while (IsBusy(ctx))
        {
            Work();

            // If we are here, then there are still remaining jobs that work() couldn't pick up.
            //	In this case those jobs are not standing by on a queue but currently executing
            //	on other threads, so they cannot be picked up by this thread.
            //	Allow to swap out this thread by OS to not spin endlessly for nothing
            if (++spin < 200)
                _mm_pause();
            else
                std::this_thread::yield();
        }
    }
}
