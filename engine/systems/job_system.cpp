#include <thread>
#include <semaphore>
#include <mutex>
#include <condition_variable>
#ifdef _WIN32
#include <Windows.h>
#endif
#include "core/threadsafe_queue.h"
#include "core/platform.h"
#include "core/logger.h"
#include "core/non_copyable.h"
#include "systems/job_system.h"

namespace cyb::jobsystem
{
    struct Job : private MovableNonCopyable
    {
        std::function<void(JobArgs)> task;
        Context* ctx{ nullptr };
        uint32_t groupJobOffset{ 0 };
        uint32_t groupJobEnd{ 0 };

        uint32_t Execute()
        {
            for (uint32_t i = groupJobOffset; i < groupJobEnd; ++i)
            {
                JobArgs args{};
                args.jobIndex = i;
                args.groupIndex = i - groupJobOffset;
                args.isFirstJobInGroup = (i == groupJobOffset);
                args.isLastJobInGroup = (i == groupJobEnd - 1);
                task(std::move(args));
            }

            return ctx->remainingJobCount.fetch_sub(1, std::memory_order_acq_rel);
        }
    };

    static constexpr size_t JOB_QUEUE_MAX_SIZE = 1024;
    using JobQueue = ThreadSafeCircularQueue<Job, JOB_QUEUE_MAX_SIZE>;

    // This structure is responsible to stop worker thread loops
    // once this is destroyed, worker threads will be woken up and end their loops.
    struct InternalState
    {
        uint32_t numCores{ 0 };
        uint32_t numThreads{ 0 };
        std::thread::id mainThreadId;
        std::vector<JobQueue> jobQueuePerThread;
        std::counting_semaphore<> wakeSemaphore{ 0 };
        std::atomic<uint32_t> nextQueue{ 0 };
        std::vector<std::jthread> threads;
        std::mutex waitMutex;
        std::condition_variable waitCondition;

        void Submit(Job&& job)
        {
            // If jobsystem is not initilized, execute the job immediately here.
            if (numThreads == 0)
            {
                job.Execute();
                return;
            }

            auto& queue = jobQueuePerThread[nextQueue.fetch_add(1) % numThreads];

            // If the queue is full, execute the job immidietly on the main thread.
            if (!queue.PushBack(std::move(job)))
            {
                assert(0);          // break if debug mode
                job.Execute();
                return;
            }
            wakeSemaphore.release();
        }

        ~InternalState()
        {
            for (auto& thread : threads)
                thread.request_stop();

            wakeSemaphore.release(wakeSemaphore.max());

            for (auto& thread : threads)
                thread.join();
        }
    };

    static InternalState internal_state{};

    // Start working on a job queue. After the job queue is finished, it 
    // can switch to an other queue and steal jobs from there.
    static void Work(uint32_t startingQueue)
    {
        for (uint32_t i = 0; i < internal_state.numThreads; ++i)
        {
            JobQueue& queue = internal_state.jobQueuePerThread[(startingQueue + i) % internal_state.numThreads];
            while (auto job = queue.PopFront())
            {
                const uint32_t progressBefore = job->Execute();

                // If progressBefore is 1, previous job was the last one and we
                // can wake up the waiting threads.
                if (progressBefore == 1)
                {
                    std::unique_lock<std::mutex> lock(internal_state.waitMutex);
                    internal_state.waitCondition.notify_all();
                }
            }
        }
    }

    void Initialize()
    {
        assert(internal_state.numThreads == 0 && "allready initialized");

        // Get number of cores on system and and use that to set number of thread
        // saving one for the main thread.
        internal_state.numCores = std::thread::hardware_concurrency();
        internal_state.numThreads = std::max(1u, internal_state.numCores - 1);
        {
            std::vector<JobQueue> temp(internal_state.numThreads);
            internal_state.jobQueuePerThread = std::move(temp);
        }
        internal_state.mainThreadId = std::this_thread::get_id();

        internal_state.threads.reserve(internal_state.numThreads);
        for (uint32_t threadID = 0; threadID < internal_state.numThreads; ++threadID)
        {
            std::jthread& worker = internal_state.threads.emplace_back([threadID](const std::stop_token stopToken) {
                while (!stopToken.stop_requested())
                {
                    Work(threadID);
                    internal_state.wakeSemaphore.acquire();
                }
            });

#if defined(_WIN32)
            HANDLE handle = (HANDLE)worker.native_handle();
            std::wstring wthreadname = std::format(L"cyb::thread_{}", threadID);
            HRESULT hr = SetThreadDescription(handle, wthreadname.c_str());
            assert(SUCCEEDED(hr));

            // Set thread affinity, each thread to a specific core starting from
            // second core (core 0 is the main thread).
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
        // Context state is updated.
        ctx.remainingJobCount.fetch_add(1);

        Job job;
        job.ctx = &ctx;
        job.task = task;
        job.groupJobOffset = 0;
        job.groupJobEnd = 1;

        internal_state.Submit(std::move(job));
    }

    // Calculate the amount of job groups to dispatch (overestimate, or "ceil").
    [[nodiscard]] static uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize)
    {
        return (jobCount + groupSize - 1) / groupSize;
    }

    uint32_t Dispatch(Context& ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)>& task)
    {
        if (jobCount == 0 || groupSize == 0)
            return 0;

        const uint32_t groupCount = DispatchGroupCount(jobCount, groupSize);

        // Context state is updated.
        ctx.remainingJobCount.fetch_add(groupCount);

        for (uint32_t groupID = 0; groupID < groupCount; ++groupID)
        {
            // For each group, generate one real job.
            Job job;
            job.ctx = &ctx;
            job.task = task;
            job.groupJobOffset = groupID * groupSize;
            job.groupJobEnd = std::min(job.groupJobOffset + groupSize, jobCount);

            job.Execute();
        }

        return groupCount;
    }

    bool IsBusy(const Context& ctx)
    {
        // Whenever the context label is greater than zero, it means that there is
        // still work that needs to be done.
        return ctx.remainingJobCount.load(std::memory_order_acquire) > 0;
    }

    void Wait(const Context& ctx)
    {
        if (IsBusy(ctx))
        {
            const bool isWorkerThread = (std::this_thread::get_id() != internal_state.mainThreadId);
            if (ctx.allowWorkOnMainThread || isWorkerThread)
                Work(internal_state.nextQueue.fetch_add(1) % internal_state.numThreads);

            while (IsBusy(ctx))
            {
                // Put thread to sleep until waitCondition is signaled.
                std::unique_lock<std::mutex> lock(internal_state.waitMutex);
                internal_state.waitCondition.wait(lock, [&ctx] { return !IsBusy(ctx); });
            }
        }
    }
}
