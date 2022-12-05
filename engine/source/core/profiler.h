#pragma once
#include "core/timer.h"
#include "core/logger.h"
#include <unordered_map>

// On/Off profiler switch
#define CYB_ENABLE_PROFILER 1

#if CYB_ENABLE_PROFILER
#if defined(__GNUC__) 
#define CYB_FUNC_SIG __PRETTY_FUNCTION__
#define CUB_FUNC_SIG __PRETTY_FUNCTION__
#elif (defined(__FUNCSIG__) || (_MSC_VER))
#define CYB_FUNC_SIG __FUNCTION__
#else
#define CYB_FUNC_SIG "CYB_FUNC_SIG"
#endif

#define CYB_PROFILE_SCOPE_LINE2(name, line) cyb::profiler::ScopedCpuEntry scoped_profile_entry##line(name)
#define CYB_PROFILE_SCOPE_LINE(name, line) CYB_PROFILE_SCOPE_LINE2(name, line)
#define CYB_PROFILE_SCOPE(name) CYB_PROFILE_SCOPE_LINE(name, __LINE__)
#define CYB_PROFILE_FUNCTION() CYB_PROFILE_SCOPE(CYB_FUNC_SIG)

#define CYB_TIMED_FUNCTION_LINE2(name, line) cyb::profiler::ScopedTimedFunction scoopTimedFunction##line(name) 
#define CYB_TIMED_FUNCTION_LINE(name, line) CYB_TIMED_FUNCTION_LINE2(name, line)
#define CYB_TIMED_FUNCTION() CYB_TIMED_FUNCTION_LINE(CYB_FUNC_SIG, __LINE__)
#else
#define CYB_PROFILE_SCOOP(name) 
#define CYB_PROFILE_FUNCTION()
#define CYB_TIMED_FUNCTION()
#endif

namespace cyb::profiler
{
    using EntryID = size_t;

    constexpr uint32_t AVERAGE_COUNTER_SAMPLES = 20;

    struct Entry
    {
        bool in_use = false;
        std::string name;
        float times[AVERAGE_COUNTER_SAMPLES] = {};
        int avg_counter = 0;
        float time = 0.0f;
        Timer cpu_timer;
    };

    void BeginFrame();
    void EndFrame();

    EntryID BeginCpuEntry(const char* name);
    void EndCpuEntry(EntryID id);

    struct ScopedCpuEntry
    {
        EntryID id;
        ScopedCpuEntry(const char* name)
        {
            id = BeginCpuEntry(name);
        }
        ~ScopedCpuEntry()
        {
            EndCpuEntry(id);
        }
    };

    struct ScopedTimedFunction
    {
        Timer timer;
        std::string name;
        ScopedTimedFunction(const std::string& functionName)
        {
            name = functionName;
            timer.Record();
        }
        ~ScopedTimedFunction()
        {
            CYB_TRACE("{0} finished in {1:.2f}ms", name, timer.ElapsedMilliseconds());
        }
    };

    const std::unordered_map<size_t, Entry>& GetData();
}