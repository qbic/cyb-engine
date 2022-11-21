#pragma once
#include <chrono>

class Timer
{
private:
    std::chrono::high_resolution_clock::time_point timestamp;

public:
    Timer()
    {
        Record();
    }

    void Record()
    {
        timestamp = std::chrono::high_resolution_clock::now();
    }

    double ElapsedSeconds()
    {
        auto timestamp2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(timestamp2 - timestamp);
        return time_span.count();
    }

    double ElapsedMilliseconds()
    {
        return ElapsedSeconds() * 1000.0;
    }
};