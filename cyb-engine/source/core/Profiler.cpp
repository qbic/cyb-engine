#include <mutex>
#include <algorithm>
#include "profiler.h"
#include "helper.h"

namespace cyb::profiler
{
    std::unordered_map<size_t, Entry> profiler_entries;
    EntryID cpu_frame = 0;

    void BeginFrame()
    {
        //cpu_frame = BeginCpuEntry("CPU Frame");
    }

    void EndFrame()
    {
        //EndCpuEntry(cpu_frame);

        for (auto& x : profiler_entries)
        {
            auto& entry = x.second;

            entry.times[entry.avg_counter++ % _countof(entry.times)] = entry.time;
            if (entry.avg_counter > _countof(entry.times))
            {
                float avg_time = 0.0f;
                for (int i = 0; i < _countof(entry.times); ++i)
                {
                    avg_time += entry.times[i];
                }

                entry.time = avg_time / _countof(entry.times);
            }

            entry.in_use = false;
        }
    }

    EntryID BeginCpuEntry(const char* name)
    {
        EntryID id = helper::StringHash(name);
        assert(!profiler_entries[id].in_use);

        profiler_entries[id].in_use = true;
        profiler_entries[id].name = name;
        profiler_entries[id].cpu_timer.Record();

        return id;
    }

    void EndCpuEntry(EntryID id)
    {
        auto it = profiler_entries.find(id);
        if (it != profiler_entries.end())
        {
            it->second.time = (float)it->second.cpu_timer.ElapsedMilliseconds();
        }
        else
        {
            assert(0);
        }
    }

    const std::unordered_map<size_t, Entry>& GetData()
    {
        return profiler_entries;
    }
}
