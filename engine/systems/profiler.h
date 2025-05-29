#pragma once
#include <unordered_map>
#include "core/timer.h"
#include "graphics/device.h"

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

#define CYB_PROFILE_CPU_SCOPE_LINE2(name, line) cyb::profiler::ScopedCpuEntry scopedCPUProfileEntry##line(name)
#define CYB_PROFILE_CPU_SCOPE_LINE(name, line) CYB_PROFILE_CPU_SCOPE_LINE2(name, line)
#define CYB_PROFILE_CPU_SCOPE(name) CYB_PROFILE_CPU_SCOPE_LINE(name, __LINE__)
#define CYB_PROFILE_FUNCTION() CYB_PROFILE_CPU_SCOPE(CYB_FUNC_SIG)

#define CYB_PROFILE_GPU_SCOPE_LINE2(name, cmd, line) cyb::profiler::ScopedGpuEntry scopedGPUPprofileEntry##line(name, cmd)
#define CYB_PROFILE_GPU_SCOPE_LINE(name, cmd, line) CYB_PROFILE_GPU_SCOPE_LINE2(name, cmd, line)
#define CYB_PROFILE_GPU_SCOPE(name, cmd) CYB_PROFILE_GPU_SCOPE_LINE(name, cmd, __LINE__)

#define CYB_TIMED_FUNCTION_LINE2(name, line) cyb::profiler::ScopedTimedFunction scoopTimedFunction##line(name) 
#define CYB_TIMED_FUNCTION_LINE(name, line) CYB_TIMED_FUNCTION_LINE2(name, line)
#define CYB_TIMED_FUNCTION() CYB_TIMED_FUNCTION_LINE(CYB_FUNC_SIG, __LINE__)
#else
#define CYB_PROFILE_CPU_SCOPE(name)
#define CYB_PROFILE_GPU_SCOPE(name, cmd)
#define CYB_PROFILE_FUNCTION()
#define CYB_TIMED_FUNCTION()
#endif // CYB_ENABLE_PROFILER

namespace cyb::profiler
{
    using EntryId = size_t;

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

        rhi::CommandList cmd;
        int gpuBegin[rhi::GraphicsDevice::GetBufferCount() + 1];
        int gpuEnd[rhi::GraphicsDevice::GetBufferCount() + 1];

        bool IsCPUEntry() const { return !cmd.IsValid(); }
        bool IsGPUEntry() const { return cmd.IsValid(); }
    };

    struct Context
    {
        std::unordered_map<EntryId, Entry> entries;
        EntryId cpuFrame = 0;
        EntryId gpuFrame = 0;
        float cpuFrameGraph[FRAME_GRAPH_ENTRIES] = {};
        float gpuFrameGraph[FRAME_GRAPH_ENTRIES] = {};

        [[nodiscard]] EntryId GetUniqueId(const std::string& name) const;
    };

    void BeginFrame();
    void EndFrame(rhi::CommandList cmd);

    [[nodiscard]] EntryId BeginCpuEntry(const std::string& name);
    [[nodiscard]] EntryId BeginGpuEntry(const std::string& name, rhi::CommandList cmd);

    void EndEntry(EntryId id);

    class ScopedCpuEntry
    {
    public:
        ScopedCpuEntry(const std::string& name);
        ~ScopedCpuEntry();

    private:
        EntryId m_id;
    };

    class ScopedGpuEntry
    {
    public:
        ScopedGpuEntry(const std::string& name, rhi::CommandList cmd);
        ~ScopedGpuEntry();

    private:
        EntryId m_id;
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

    const Context& GetContext() noexcept;
}