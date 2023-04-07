#pragma once
#include <unordered_map>
#include "core/timer.h"
#include "graphics/graphics-device.h"

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

#define CYB_PROFILE_SCOPE_LINE2(name, line) cyb::profiler::ScopedCpuEntry scopedCPUProfileEntry##line(name)
#define CYB_PROFILE_SCOPE_LINE(name, line) CYB_PROFILE_SCOPE_LINE2(name, line)
#define CYB_PROFILE_SCOPE(name) CYB_PROFILE_SCOPE_LINE(name, __LINE__)
#define CYB_PROFILE_FUNCTION() CYB_PROFILE_SCOPE(CYB_FUNC_SIG)

#define CYB_PROFILE_GPU_SCOPE_LINE2(name, cmd, line) cyb::profiler::ScopedGpuEntry scopedGPUPprofileEntry##line(name, cmd)
#define CYB_PROFILE_GPU_SCOPE_LINE(name, cmd, line) CYB_PROFILE_GPU_SCOPE_LINE2(name, cmd, line)
#define CYB_PROFILE_GPU_SCOPE(name, cmd) CYB_PROFILE_GPU_SCOPE_LINE(name, cmd, __LINE__)

#define CYB_TIMED_FUNCTION_LINE2(name, line) cyb::profiler::ScopedTimedFunction scoopTimedFunction##line(name) 
#define CYB_TIMED_FUNCTION_LINE(name, line) CYB_TIMED_FUNCTION_LINE2(name, line)
#define CYB_TIMED_FUNCTION() CYB_TIMED_FUNCTION_LINE(CYB_FUNC_SIG, __LINE__)
#else
#define CYB_PROFILE_SCOOP(name) 
#define CYB_PROFILE_FUNCTION()
#define CYB_PROFILE_GPU_SCOPE(name, cmd)
#define CYB_TIMED_FUNCTION()
#endif

namespace cyb::profiler
{
    using EntryID = size_t;

    constexpr uint32_t AVERAGE_COUNTER_SAMPLES = 20;
    constexpr uint32_t FRAME_GRAPH_ENTRIES = 144;

    struct Entry
    {
        bool inUse = false;
        std::string name;
        float times[AVERAGE_COUNTER_SAMPLES] = {};
        int avgCounter = 0;
        float time = 0.0f;
        Timer cpuTimer;

        graphics::CommandList cmd;
        int gpuBegin[graphics::GraphicsDevice::GetBufferCount() + 1];
        int gpuEnd[graphics::GraphicsDevice::GetBufferCount() + 1];
        bool IsCPUEntry() const { return !cmd.IsValid(); }
    };

    struct Context
    {
        std::unordered_map<EntryID, Entry> entries;
        EntryID cpuFrame;
        EntryID gpuFrame;
        float cpuFrameGraph[FRAME_GRAPH_ENTRIES] = {};
        float gpuFrameGraph[FRAME_GRAPH_ENTRIES] = {};
    };

    void BeginFrame();
    void EndFrame(graphics::CommandList cmd);

    EntryID BeginCpuEntry(const char* name);
    EntryID BeginGpuEntry(const char* name, graphics::CommandList cmd);

    void EndEntry(EntryID id);

    class ScopedCpuEntry
    {
    public:
        ScopedCpuEntry(const char* name);
        ~ScopedCpuEntry();

    private:
        EntryID m_id;
    };

    class ScopedGpuEntry
    {
    public:
        ScopedGpuEntry(const char* name, graphics::CommandList cmd);
        ~ScopedGpuEntry();

    private:
        EntryID m_id;
    };

    class ScopedTimedFunction
    {
    public:
        ScopedTimedFunction(const std::string& name);
        ~ScopedTimedFunction();

    private:
        Timer m_timer;
        std::string m_name;
    };

    const Context& GetContext();
}