#pragma once
#include <chrono>

namespace cyb
{
    // High-resolution timer used for application timing and profiling.
    class Timer
    {
    public:
        Timer() noexcept
        {
            Record();
        }

        // Record a new timestamp to measure elapsed time from.
        void Record() noexcept
        {
            timestamp_ = std::chrono::high_resolution_clock::now();
        }

        // Get elapsed seconds from a specified timepoint.
        [[nodiscard]] double ElapedSecondsSence(std::chrono::high_resolution_clock::time_point time) const noexcept
        {
            std::chrono::duration<double> timeSpan = std::chrono::duration_cast<std::chrono::duration<double>>(time - timestamp_);
            return timeSpan.count();
        }

        // Get elapsed time sence last cecorded timestamp in seconds.
        [[nodiscard]] double ElapsedSeconds() const noexcept
        {
            return ElapedSecondsSence(std::chrono::high_resolution_clock::now());
        }

        // Get elapsed time sence last cecorded timestamp in milliseconds.
        [[nodiscard]] double ElapsedMilliseconds() const noexcept
        {
            return ElapsedSeconds() * 1000.0;
        }

        // Get elapsed time sence the last cecorded timestamp in seconds and 
        // move timestamp to the current time.
        [[nodiscard]] double RecordElapsedSeconds() noexcept
        {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = ElapedSecondsSence(now);
            timestamp_ = now;
            return elapsed;
        }

    private:
        std::chrono::high_resolution_clock::time_point timestamp_;
    };
} // namespace cyb