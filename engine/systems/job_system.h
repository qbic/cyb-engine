/**
 * 
 * @mainpage cyb::jobsystem - work-stealing job system
 *
 * Runs a fixed pool of worker threads (hardware threads minus one, capped at 63).
 * Each worker is pinned to its own core starting at core 1, which leaves core 0
 * for the main thread.
 *
 * Each worker owns a bounded MPMC queue. Jobs are pushed round-robin across the
 * workers. An idle worker pops from its own queue first, then steals from the
 * other workers, and sleeps on a semaphore when it finds no work. If the target
 * queue is full, the job runs inline on the thread that submitted it.
 *
 * JobCounter tracks completion. Execute() and Dispatch() increment the counter,
 * and each finished job (or job group) decrements it. Wait() does not block
 * idly: the calling thread helps execute queued jobs until the counter reaches
 * zero.
 *
 * Usage:
 *   jobsystem::Initialize();
 *
 *   jobsystem::JobCounter counter;
 *   jobsystem::Dispatch(counter, (uint32_t)items.size(), 64,
 *       [&](const jobsystem::JobArgs& args) { Process(items[args.jobIndex]); });
 *   jobsystem::Wait(counter);
 *
 * Notes:
 *   - Call Initialize() before any other function in this namespace.
 *   - A JobCounter must outlive every job submitted against it.
 *   - Tasks are stored as std::function and copied per job/group, so keep
 *     lambda captures small.
 */
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>

namespace cyb::jobsystem
{
    struct JobArgs
    {
        uint32_t jobIndex;
        uint32_t groupIndex;
        bool isFirstJobInGroup;
        bool isLastJobInGroup;
    };

    // Counter for tracking remaining jobs. Can be Waited on.
    struct JobCounter
    {
        std::atomic<uint32_t> remainingJobCount{ 0 };
    };

    using JobTask = std::function<void(const JobArgs&)>;
    
    /**
     * @brief Initialize the jobsystem.
     * Must be called before any other calls in the subsystem.
     * This will spawn (number of available cores minus one) worker threads, assigning
     * each of them to a seperate core starting from one (leaving zero for main thread).
     */
    void Initialize() noexcept;

    /**
     * @return Number of worker threads utilized by the jobsystem.
     */
    [[nodiscard]] uint32_t WorkerCount() noexcept;

    /**
     * @brief Execute a task async.
     * @param target Counter to decrement when this job finishes.
     * @param task Entry point invoked when executed.
     */
    void Execute(JobCounter& target, const JobTask& task) noexcept;

    /**
     * Create a set of jobs and distribute work among the available threads.
     * @param target Counter to decrement when a group job finishes.
     * @param count Total number of jobs to dispatch.
     * @param groupSize Number of jobs to pass as a group to each thread.
     * @param task Entry point invoked when the job runs.
     * @return The number of actual jobs groups created.
     */
    uint32_t Dispatch(JobCounter& target, uint32_t count, uint32_t groupSize, const JobTask& task) noexcept;
    
    /**
     * Create a set of jobs and distribute work among the available threads.
     * Will block until all jobs are executed.
     * @param count Total number of jobs to dispatch.
     * @param groupSize Number of jobs to pass as a group to each thread.
     * @param task Entry point invoked when the job runs.
     * @return The number of actual jobs groups created.
     */
    uint32_t Dispatch(uint32_t count, uint32_t groupSize, const JobTask& task) noexcept;
    
    /**
     * @return True if the counter has reached zero.
     * @param counter Counter to wait on.
     */
    bool IsFinished(const JobCounter& counter) noexcept;

    /**
     * @brief Blocks until counter reaches zero.
     * @param target Counter to wait on.
     */
    void Wait(const JobCounter& target) noexcept;
}
