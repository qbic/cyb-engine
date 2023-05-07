#include <mutex>
#include <algorithm>
#include <numeric>
#include "core/profiler.h"
#include "core/hash.h"
#include "core/logger.h"

namespace cyb::profiler
{
    Context context;

    bool initialized = false;
    graphics::GPUQuery query;
    graphics::GPUBuffer queryResultBuffer[graphics::GraphicsDevice::GetBufferCount() + 1];
    uint32_t queryCount = 0;
    uint32_t queryIndex = 0;

    void BeginFrame()
    {
        if (!initialized)
        {
            initialized = true;
            graphics::GraphicsDevice* device = graphics::GetDevice();

            graphics::GPUQueryDesc desc;
            desc.type = graphics::GPUQueryType::Timestamp;
            desc.queryCount = 1024;
            bool success = device->CreateQuery(&desc, &query);
            assert(success);

            graphics::GPUBufferDesc bd;
            bd.usage = graphics::MemoryAccess::Readback;
            bd.size = desc.queryCount * sizeof(uint64_t);

            for (uint32_t i = 0; i < _countof(queryResultBuffer); ++i)
            {
                success = device->CreateBuffer(&bd, nullptr, &queryResultBuffer[i]);
                assert(success);
            }
        }

        context.cpuFrame = BeginCpuEntry("CPU Frame");

        graphics::GraphicsDevice* device = graphics::GetDevice();
        graphics::CommandList cmd = device->BeginCommandList();
        device->ResetQuery(&query, 0, query.desc.queryCount, cmd);
        context.gpuFrame = BeginGpuEntry("GPU Frame", cmd);
    }

    void EndFrame(graphics::CommandList cmd)
    {
        assert(initialized);

        graphics::GraphicsDevice* device = graphics::GetDevice();

        // Read the GPU Frame end range manually because it will be on a separate 
        // command list than start point
        Entry& gpuEntry = context.entries[context.gpuFrame];
        gpuEntry.gpuEnd[queryIndex] = queryCount++;
        device->EndQuery(&query, gpuEntry.gpuEnd[queryIndex], cmd);

        EndEntry(context.cpuFrame);

        double gpuFrequency = (double)device->GetTimestampFrequency() / 1000.0;
        device->ResolveQuery(&query, 0, queryCount, &queryResultBuffer[queryIndex], 0ull, cmd);
        queryCount = 0;
        queryIndex = (queryIndex + 1) % _countof(queryResultBuffer);
        uint64_t* queryResults = (uint64_t*)queryResultBuffer[queryIndex].mappedData;

        for (auto& [entryID, entry] : context.entries)
        {
            if (!entry.IsCPUEntry())
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

            // Recalculate average time
            entry.times[entry.avgCounter++ % AVERAGE_COUNTER_SAMPLES] = entry.time;
            if (entry.avgCounter > AVERAGE_COUNTER_SAMPLES)
                entry.time = std::accumulate(std::begin(entry.times), std::end(entry.times), 0.0f) / AVERAGE_COUNTER_SAMPLES;

            // Remove inUse flag so entry can be used again next frame
            entry.inUse = false;
        }

        // Update frame graph arrays
        for (uint32_t i = 0; i < FRAME_GRAPH_ENTRIES - 1; ++i)
        {
            context.cpuFrameGraph[i] = context.cpuFrameGraph[i + 1];
            context.gpuFrameGraph[i] = context.gpuFrameGraph[i + 1];
        }
        context.cpuFrameGraph[FRAME_GRAPH_ENTRIES - 1] = context.entries[context.cpuFrame].time;
        context.gpuFrameGraph[FRAME_GRAPH_ENTRIES - 1] = context.entries[context.gpuFrame].time;
    }

    EntryId BeginCpuEntry(const std::string& name)
    {
        EntryId id = hash::String(name);
        size_t differentiator = 0;
        while (context.entries[id].inUse)
            hash::Combine(id, differentiator++);

        context.entries[id].inUse = true;
        context.entries[id].name = name;
        context.entries[id].cpuTimer.Record();

        return id;
    }

    EntryId BeginGpuEntry(const std::string& name, graphics::CommandList cmd)
    {
        EntryId id = hash::String(name);
        size_t differentiator = 0;
        while (context.entries[id].inUse)
            hash::Combine(id, differentiator++);

        context.entries[id].inUse = true;
        context.entries[id].name = name;
        context.entries[id].cmd = cmd;
        context.entries[id].gpuBegin[queryIndex] = queryCount++;

        graphics::GetDevice()->EndQuery(&query, context.entries[id].gpuBegin[queryIndex], cmd);
        return id;
    }

    void EndEntry(EntryId id)
    {
        auto it = context.entries.find(id);
        assert(it != context.entries.end());

        Entry& entry = it->second;
        if (entry.IsCPUEntry())
        {
            entry.time = (float)entry.cpuTimer.ElapsedMilliseconds();
        }
        else
        {
            entry.gpuEnd[queryIndex] = queryCount++;
            graphics::GetDevice()->EndQuery(&query, entry.gpuEnd[queryIndex], entry.cmd);
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

    ScopedGpuEntry::ScopedGpuEntry(const std::string& name, graphics::CommandList cmd)
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

    const Context& GetContext()
    {
        return context;
    }
}
