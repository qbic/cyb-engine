#pragma once
#include <chrono>

class Timer
{
public:
    Timer()
    {
        Record();
    }

    inline void Record()
    {
        m_timestamp = std::chrono::high_resolution_clock::now();
    }

    inline double ElapedSecondsSence(std::chrono::high_resolution_clock::time_point time) const
    {
        std::chrono::duration<double> timeSpan = std::chrono::duration_cast<std::chrono::duration<double>>(time - m_timestamp);
        return timeSpan.count();
    }

    inline double ElapsedSeconds() const
    {
        return ElapedSecondsSence(std::chrono::high_resolution_clock::now());
    }

    inline double ElapsedMilliseconds() const
    {
        return ElapsedSeconds() * 1000.0;
    }

    inline double RecordElapsedSeconds()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = ElapedSecondsSence(now);
        m_timestamp = now;
        return elapsed;
    }

private:
    std::chrono::high_resolution_clock::time_point m_timestamp;
};