#pragma once
#include <atomic>
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

    // Initialize the jobsystem.
    // Must be called before any other calls in the subsystem.
    // This will spawn (number of available cores minus one) worker threads, assigning
    // each of them to a seperate core starting from one (leaving zero for main thread).
    void Initialize() noexcept;

    // Get number of worker threads utilized by the jobsystem.
    [[nodiscard]] uint32_t WorkerCount() noexcept;

    /**
     * @brief Execute a task async, the context can be waited on.
     *        If jobsystem hasn't been initialized this will be immidietly executed.
     */
    void Execute(JobCounter& counter, const JobTask& task) noexcept;

    /*
     * Create a set of jobs and distribute work among the available threads.
     * Task gets copied to all jobs, so keep lamda captures as small as possible.
     * 
     * Example usage to distribute workload of a vector with 6 elements into 3 groups:
     * std::vector<int> test = {{ 1, 2, 3, 4, 5, 6 }};
     * Dispatch(ctx, test.size(), 2, [&test] (jobsystem::JobArgs args) {
     *     int& value = test[args.jobIndex];
     * }
     * 
     * @param jobCount Total number of jobs to dispatch.
     * @param groupSize Number of jobs to pass as a group to each thread.
     * @return The number of actual jobs groups created.
     */
    uint32_t Dispatch(JobCounter& counter, uint32_t count, uint32_t groupSize, const JobTask& task) noexcept;
    
    // Check of the jobsystem is still working on jobs in the counter.
    // @return True if the counter has reached zero.
    bool IsFinished(const JobCounter& counter) noexcept;

    // Waits until counter reaches zero.
    void Wait(const JobCounter& counter) noexcept;
}
