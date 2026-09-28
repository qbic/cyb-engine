#include "core/mpmc_queue.h"
#include "core/sys.h"
#include "core/non_copyable.h"
#include "systems/job_system.h"
#include <array>
#include <thread>
#include <semaphore>
#include <cassert>
#include <random>

namespace cyb::jobsystem
{
    struct Job : private MovableNonCopyable
    {
        JobTask task;
        JobCounter* counter = nullptr;
        uint32_t groupJobOffset = 0;
        uint32_t groupJobEnd = 0;
    };

    static constexpr uint32_t MAX_WORKER_COUNT = 63;
    static constexpr uint32_t SPIN_ITERATIONS_BEFORE_WAIT = 32;
    static constexpr size_t JOB_QUEUE_MAX_SIZE = 1024;
    using JobQueue = MPMCQueue<Job, JOB_QUEUE_MAX_SIZE>;

    struct WorkerData
    {
        JobQueue queue;
        std::jthread thread;
    };

    struct JobSystemImpl
    {
        //using rng_engine = std::mt19937_64;
        using rng_engine = std::minstd_rand;
        std::vector<WorkerData> workers;
        std::counting_semaphore<> wakeSignal{ 0 };
        std::atomic<uint32_t> nextPushWorker{ 0 };
        std::array<uint8_t, 256> modLut{}; // Lookup table from atomic uint8_t->workerIndex (avoiding modulo)

        static thread_local uint32_t tl_workerIndex;
        static thread_local rng_engine tl_rng;

        JobSystemImpl(uint32_t workerCount) :
            workers(workerCount)
        {
            // Precompute lookup table of modulos to avoid divs at runtime
            for (uint32_t i = 0; i < modLut.size(); ++i)
                modLut[i] = static_cast<uint8_t>(i % workers.size());

            // Spawn worker threads
            for (uint32_t threadID = 0; threadID < workers.size(); ++threadID)
            {
                WorkerData& worker = workers[threadID];
                worker.thread = std::jthread{ [this, threadID](std::stop_token stopToken) {
                    // Name all worker threads for easier debugging and assign them to
                    // their own CPU core. This isn't strictly necessary, but can
                    // improve performance and makes debugging easier.
                    SetThisThreadName(std::format("cyb_worker_{}", threadID));
                    SetThisThreadAffinity(BIT(threadID + 1));

                    WorkerThreadMain(threadID, stopToken);
                } };
            }
        }

        ~JobSystemImpl()
        {
            for (auto& worker : workers)
                worker.thread.request_stop();

            wakeSignal.release(static_cast<ptrdiff_t>(workers.size()));

            for (auto& worker : workers)
            {
                if (worker.thread.joinable())
                    worker.thread.join();
            }
        }

        constexpr uint8_t ConstrainQueueIndex(uint32_t index) const noexcept
        {
            return modLut[static_cast<uint8_t>(index)];
        }

        void Schedule(Job&& job) noexcept
        {
            // Schedule job's evenly through all the available workers
            // to avoid as much stealing contention as possible
            const uint32_t index = nextPushWorker.fetch_add(1, std::memory_order_relaxed);
            WorkerData& worker = workers[ConstrainQueueIndex(index)];
            if (!worker.queue.Push(job))
            {
                Execute(std::move(job));
                return;
            }

            // Signal work available
            wakeSignal.release();
        }

        void Execute(Job&& job) noexcept
        {
            for (uint32_t i = job.groupJobOffset; i < job.groupJobEnd; ++i)
            {
                JobArgs args{};
                args.jobIndex = i;
                args.groupIndex = i - job.groupJobOffset;
                args.isFirstJobInGroup = (i == job.groupJobOffset);
                args.isLastJobInGroup = (i == job.groupJobEnd - 1);
                job.task(args);
            }

            // If no other jobs are remaining, wake up any threads waiting
            // on this job counter
            const uint32_t remaining = job.counter->remainingJobCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (remaining == 0)
                job.counter->remainingJobCount.notify_all();
        }

        [[nodiscard]] WorkerData& CurrentWorker() noexcept
        {
            return workers[tl_workerIndex];
        }

        [[nodiscard]] bool GetJobToExecute(Job& job) noexcept
        {
            WorkerData& worker = CurrentWorker();
            if (worker.queue.Pop(job))
                return true;

            uint8_t victim = ConstrainQueueIndex(tl_rng());
            if (victim == tl_workerIndex)
                victim = ConstrainQueueIndex(victim + 1);

            return workers[victim].queue.Pop(job);
        }

        [[nodiscard]] bool ExecuteOneJob() noexcept
        {
            Job job{};
            if (!GetJobToExecute(job))
                return false;

            Execute(std::move(job));
            return true;
        }

        // No thread local reads here sence this might be called from a 
        // non-worker thread through Wait()
        [[nodiscard]] bool StealAndExecuteExhaustive(uint32_t startingQueue, uint32_t queueCount) noexcept
        {
            Job job{};
            for (uint32_t i = 0; i < queueCount; ++i)
            {
                const uint8_t victim = ConstrainQueueIndex(startingQueue + i);
                if (workers[victim].queue.Pop(job))
                {
                    Execute(std::move(job));
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool ExecuteOneJobExhaustive() noexcept
        {
            // Try to execute job on worker's own queue
            Job job{};
            WorkerData& worker = CurrentWorker();
            if (worker.queue.Pop(job))
            {
                Execute(std::move(job));
                return true;
            }

            // Try to steal and execute a job from another
            // worker thread, skipping the current one
            return StealAndExecuteExhaustive(tl_workerIndex + 1, workers.size() - 1);
        }

        void WaitUntilWorkAvailable() noexcept
        {
            if (ExecuteOneJobExhaustive())
                return;

            wakeSignal.acquire();
        }

        void ExecuteWorkOrWait() noexcept
        {
            for (uint32_t i = 0; i < SPIN_ITERATIONS_BEFORE_WAIT; ++i)
            {
                if (ExecuteOneJob())
                    return;
                std::this_thread::yield();
            }

            WaitUntilWorkAvailable();
        }

		void WorkerThreadMain(uint32_t workerIndex, std::stop_token stopToken) noexcept
		{
            tl_workerIndex = workerIndex;
            tl_rng.seed(workerIndex + 1);

            while (!stopToken.stop_requested())
                ExecuteWorkOrWait();
		}

		[[nodiscard]] bool IsFinished(const JobCounter& counter) const noexcept
		{
			return counter.remainingJobCount.load(std::memory_order_acquire) == 0;
		}
    };

    thread_local uint32_t JobSystemImpl::tl_workerIndex = 0;
    thread_local JobSystemImpl::rng_engine JobSystemImpl::tl_rng{};

    static std::unique_ptr<JobSystemImpl> g_jobSystem;

    void Initialize() noexcept
    {
        assert(!g_jobSystem && "only initialize once");

        // Calculate how many actual worker threads we want
        const uint32_t coreCount = std::max(1u, std::thread::hardware_concurrency());
        const uint32_t workerCount = std::clamp(coreCount - 1u, 1u, MAX_WORKER_COUNT);

		g_jobSystem = std::make_unique<JobSystemImpl>(workerCount);
    }

    uint32_t WorkerCount() noexcept
    {
        return g_jobSystem->workers.size();
    }

    void Execute(JobCounter& target, const JobTask& task) noexcept
    {
        // Update job counter
        target.remainingJobCount.fetch_add(1, std::memory_order_relaxed);

        Job job;
        job.counter = &target;
        job.task = task;
        job.groupJobOffset = 0;
        job.groupJobEnd = 1;

        g_jobSystem->Schedule(std::move(job));
    }

    // Calculate the amount of job groups to dispatch (overestimate, or "ceil").
    [[nodiscard]] static uint32_t DispatchGroupCount(uint32_t count, uint32_t groupSize) noexcept
    {
        return (count + groupSize - 1) / groupSize;
    }

    uint32_t Dispatch(JobCounter& target, uint32_t count, uint32_t groupSize, const JobTask& task) noexcept
    {
        if (count == 0 || groupSize == 0)
            return 0;

        const uint32_t groupCount = DispatchGroupCount(count, groupSize);

        // Update job counter
        target.remainingJobCount.fetch_add(groupCount, std::memory_order_relaxed);
        
        for (uint32_t groupID = 0; groupID < groupCount; ++groupID)
        {
            // For each group, generate one real job.
            Job job;
            job.counter = &target;
            job.task = task;
            job.groupJobOffset = groupID * groupSize;
            job.groupJobEnd = std::min(job.groupJobOffset + groupSize, count);

            g_jobSystem->Schedule(std::move(job));
        }

        return groupCount;
    }

    uint32_t Dispatch(uint32_t count, uint32_t groupSize, const JobTask& task) noexcept
    {
        JobCounter target{};
        uint32_t groups = Dispatch(target, count, groupSize, task);
        Wait(target);
        return groups;
    }

    bool IsFinished(const JobCounter& counter) noexcept
    {
        return g_jobSystem->IsFinished(counter);
    }

    void Wait(const JobCounter& target) noexcept
    {
        int32_t observed = 0;
        while ((observed = target.remainingJobCount.load(std::memory_order_acquire)) != 0)
        {
            if (g_jobSystem->StealAndExecuteExhaustive(0, g_jobSystem->workers.size()))
                continue;

            target.remainingJobCount.wait(observed, std::memory_order_acquire);
        }
    }
} // namespace cyb::jobsystem
