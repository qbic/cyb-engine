#include <algorithm>
#include <numeric>
#include "core/hash.h"
#include "core/logger.h"
#include "core/mutex.h"
#include "systems/profiler.h"

namespace cyb::profiler
{
    Context context;

    bool initialized = false;
    Mutex lock;
    rhi::GPUQuery query;
    std::array<rhi::GPUBuffer, rhi::GraphicsDevice::GetBufferCount()> queryResultBuffer = {};
    std::atomic<uint32_t> queryCount = 0;
    uint32_t queryIndex = 0;

    EntryId Context::GetUniqueId(const std::string& name) const
    {
        EntryId id = hash::String(name);

        // if one entry name is hit multiple times, differentiate between them!
        size_t differentiator = 0;
        ScopedMutex lck(lock);
        while (context.entries[id].inUse)
            hash::Combine(id, differentiator++);

        return id;
    }

    void BeginFrame()
    {
        if (!initialized)
        {
            initialized = true;
            rhi::GraphicsDevice* device = rhi::GetDevice();

            rhi::GPUQueryDesc desc;
            desc.type = rhi::GPUQueryType::Timestamp;
            desc.queryCount = 1024;
            bool success = device->CreateQuery(&desc, &query);
            assert(success);

            rhi::GPUBufferDesc bd;
            bd.usage = rhi::MemoryAccess::Readback;
            bd.size = desc.queryCount * sizeof(uint64_t);

            for (auto& buffer : queryResultBuffer)
            {
                success = device->CreateBuffer(&bd, nullptr, &buffer);
                assert(success);
            }
        }

        context.cpuFrame = BeginCpuEntry("CPU Frame");

        rhi::GraphicsDevice* device = rhi::GetDevice();
        rhi::CommandList cmd = device->BeginCommandList();

        const double gpuFrequency = (double)device->GetTimestampFrequency() / 1000.0;
        queryIndex = (queryIndex + 1) % queryResultBuffer.size();
        uint64_t* queryResults = (uint64_t*)queryResultBuffer[queryIndex].mappedData;

        for (auto& [entryID, entry] : context.entries)
        {
            if (entry.IsGPUEntry())
            {
                int beginQuery = entry.gpuBegin[queryIndex];
                int endQuery = entry.gpuEnd[queryIndex];
                if (queryResultBuffer[queryIndex].mappedData != nullptr && beginQuery >= 0 && endQuery >= 0)
                {
                    uint64_t beginResult = queryResults[beginQuery];
                    uint64_t endResult = queryResults[endQuery];
                    entry.time = (float)abs((double)(endResult - beginResult) / gpuFrequency);
                }
                entry.gpuBegin[queryIndex] = -1;
                entry.gpuEnd[queryIndex] = -1;
            }

            // update average frame time
            entry.times[entry.avgCounter++ % AVERAGE_COUNTER_SAMPLES] = entry.time;
            if (entry.avgCounter > AVERAGE_COUNTER_SAMPLES)
            {
                entry.time = std::accumulate(std::begin(entry.times), std::end(entry.times), 0.0f) / AVERAGE_COUNTER_SAMPLES;
            }

            entry.inUse = false;
        }

        device->ResetQuery(&query, 0, query.desc.queryCount, cmd);
        context.gpuFrame = BeginGpuEntry("GPU Frame", cmd);

        // update the frametime graph arrays used by profiler gui
        for (uint32_t i = 0; i < FRAME_GRAPH_ENTRIES - 1; ++i)
        {
            context.cpuFrameGraph[i] = context.cpuFrameGraph[i + 1];
            context.gpuFrameGraph[i] = context.gpuFrameGraph[i + 1];
        }
        context.cpuFrameGraph[FRAME_GRAPH_ENTRIES - 1] = context.entries[context.cpuFrame].time;
        context.gpuFrameGraph[FRAME_GRAPH_ENTRIES - 1] = context.entries[context.gpuFrame].time;
    }

    void EndFrame(rhi::CommandList cmd)
    {
        assert(initialized);

        rhi::GraphicsDevice* device = rhi::GetDevice();

        // read the GPU Frame end range manually because it will be on a separate 
        // command list than start point
        Entry& gpuEntry = context.entries[context.gpuFrame];
        gpuEntry.gpuEnd[queryIndex] = queryCount.fetch_add(1);
        device->EndQuery(&query, gpuEntry.gpuEnd[queryIndex], cmd);

        EndEntry(context.cpuFrame);
        device->ResolveQuery(&query, 0, queryCount.load(), &queryResultBuffer[queryIndex], 0ull, cmd);
        queryCount.store(0);
    }

    EntryId BeginCpuEntry(const std::string& name)
    {
        const EntryId id = context.GetUniqueId(name);
        Entry& entry = context.entries[id];

        entry.inUse = true;
        entry.name = name;
        entry.cpuTimer.Record();

        return id;
    }

    EntryId BeginGpuEntry(const std::string& name, rhi::CommandList cmd)
    {
        const EntryId id = context.GetUniqueId(name);
        Entry& entry = context.entries[id];

        entry.inUse = true;
        entry.name = name;
        entry.cmd = cmd;
        entry.gpuBegin[queryIndex] = queryCount.fetch_add(1);

        rhi::GetDevice()->EndQuery(&query, entry.gpuBegin[queryIndex], cmd);
        return id;
    }

    void EndEntry(EntryId id)
    {
        lock.Lock();
        auto it = context.entries.find(id);
        assert(it != context.entries.end());
        lock.Unlock();

        Entry& entry = it->second;
        if (entry.IsCPUEntry())
        {
            entry.time = (float)entry.cpuTimer.ElapsedMilliseconds();
        }
        else
        {
            entry.gpuEnd[queryIndex] = queryCount.fetch_add(1);
            rhi::GetDevice()->EndQuery(&query, entry.gpuEnd[queryIndex], entry.cmd);
        }
    }

    ScopedCpuEntry::ScopedCpuEntry(const std::string& name)
    {
        m_id = BeginCpuEntry(name);
    }

    ScopedCpuEntry::~ScopedCpuEntry()
    {
        EndEntry(m_id);
    }

    ScopedGpuEntry::ScopedGpuEntry(const std::string& name, rhi::CommandList cmd)
    {
        m_id = BeginGpuEntry(name, cmd);
    }

    ScopedGpuEntry::~ScopedGpuEntry()
    {
        EndEntry(m_id);
    }

    ScopedTimedFunction::ScopedTimedFunction(const std::string& name)
    {
        m_name = name;
        m_timer.Record();
    }

    ScopedTimedFunction::~ScopedTimedFunction()
    {
        CYB_TRACE("{0} finished in {1:.2f}ms", m_name, m_timer.ElapsedMilliseconds());
    }

    const Context& GetContext() noexcept
    {
        return context;
    }
}
