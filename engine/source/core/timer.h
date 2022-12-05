#pragma once
#include <chrono>

class Timer
{
public:
    Timer()
    {
        Record();
    }

    void Record()
    {
        m_startTime = std::chrono::high_resolution_clock::now();
    }

    double ElapsedSeconds()
    {
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> timeSpan = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - m_startTime);
        return timeSpan.count();
    }

    double ElapsedMilliseconds()
    {
        return ElapsedSeconds() * 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point m_startTime;
};