#pragma once
#include <chrono>

namespace cyb
{
    // High-resolution timer used for application timing and profiling.
    class Timer
    {
    public:
        Timer()
        {
            Record();
        }

        // Record a new timestamp to measure elapsed time from.
        void Record()
        {
            m_timestamp = std::chrono::high_resolution_clock::now();
        }

        // Get elapsed seconds from a specified timepoint.
        [[nodiscard]] double ElapedSecondsSence(std::chrono::high_resolution_clock::time_point time) const
        {
            std::chrono::duration<double> timeSpan = std::chrono::duration_cast<std::chrono::duration<double>>(time - m_timestamp);
            return timeSpan.count();
        }

        // Get elapsed time sence last cecorded timestamp in seconds.
        [[nodiscard]] double ElapsedSeconds() const
        {
            return ElapedSecondsSence(std::chrono::high_resolution_clock::now());
        }

        // Get elapsed time sence last cecorded timestamp in milliseconds.
        [[nodiscard]] double ElapsedMilliseconds() const
        {
            return ElapsedSeconds() * 1000.0;
        }

        // Get elapsed time sence the last cecorded timestamp in seconds and 
        // move timestamp to the current time.
        [[nodiscard]] double RecordElapsedSeconds()
        {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = ElapedSecondsSence(now);
            m_timestamp = now;
            return elapsed;
        }

    private:
        std::chrono::high_resolution_clock::time_point m_timestamp;
    };
}