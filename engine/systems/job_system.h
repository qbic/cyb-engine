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

    /**
     * @brief Defines a state of execution that can be waited on.
     */
    struct Context
    {
        std::atomic<uint32_t> counter{ 0 };
        bool allowWorkOnMainThread{ true };
    };

    /**
     * @brief Initialize the jobsystem. Must be called before any other calls in the subsystem.
     * 
     * This will spawn (Number of available cores)-1 threads assigning them to a seperate
     * core, saving 1 core for the main thread.
     */
    void Initialize();

    /**
     * @brief Get number of worker threads utilized by the jobsystem.
     */
    uint32_t GetThreadCount();

    /**
     * @brief Execute a task async, the context can be waited on.
     */
    void Execute(Context& ctx, const std::function<void(JobArgs)>& task);

    /**
     * @brief Create a set of jobs and distribute work among the available threads.
     * 
     * Example usage to distribute workload of a vector with 6 elements into 3 groups:
     * std::vector<int> test = {{ 1, 2, 3, 4, 5, 6 }};
     * Dispatch(ctx, test.size(), 2, [test] (jobsystem::JobArgs args) {
     *     int& value = test[args.jobIndex];
     * }
     * 
     * @param jobCount Total number of jobs to dispatch.
     * @param groupSize Number of jobs to pass as a group to each thread.
     */
    void Dispatch(Context& ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)>& task);
    
    /**
     * @brief @brief Check if context is busy with jobs.
     * @return True if context still has jobs not finished, false otherwise.
     */
    bool IsBusy(const Context& ctx);

    /**
     * @brief Wait on context until all jobs are executed.
     */
    void Wait(const Context& ctx);
}
