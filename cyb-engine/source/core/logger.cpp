#include <sstream>
#include <deque>
#include <list>
#include <typeinfo>
#include <typeindex>
#include <stdio.h>
#include <stdarg.h>
#include "core/platform.h"
#include "core/spinlock.h"
#include "core/logger.h"

namespace cyb::logger
{
    std::list<std::shared_ptr<LogOutputModule>> outputModules;
    std::deque<std::string> stream;
    SpinLock locker;
    uint32_t logLevelThreshold = LOGLEVEL_TRACE;

    void RegisterOutputModule(std::shared_ptr<LogOutputModule> output, bool writeHistory)
    {
#ifdef CYB_DEBUG_BUILD
        // In debug build, ensure there is only one instance of the output module so 
        // that there are no accidental double outputs
        const std::type_index outputIndex = std::type_index(typeid(*output));
        for (const auto& it : outputModules)
        {
            const std::type_index itIndex = std::type_index(typeid(*it));
            assert(itIndex != outputIndex);
        }
#endif

        outputModules.push_back(output);

        if (writeHistory)
        {
            for (const auto& entry : stream)
            {
                output->Write(entry.c_str());
            }
        }
    }

    void Post(uint32_t loglevel, const std::string& input)
    {
        if (loglevel < logLevelThreshold)
            return;

        locker.lock();

        std::stringstream ss("");
        switch (loglevel)
        {
        case LOGLEVEL_TRACE: ss << "[TRACE] "; break;
        case LOGLEVEL_INFO: ss << "[INFO] "; break;
        case LOGLEVEL_WARNING: ss << "[WARNING] "; break;
        case LOGLEVEL_ERROR: ss << "[ERROR] "; break;
        default: break;
        }

        ss << input << std::endl;
        stream.push_back(ss.str());

        for (auto& output : outputModules)
        {
            output->Write(ss.str());
        }

#if CYB_ERRORS_ARE_FATAL
        if (loglevel == LOGLEVEL_ERROR)
        {
            platform::MsgBox(fmt::format(fmt, std::forward<T>(args)...), "CybEngine Error");
            platform::Exit(1);
        }
#endif

        locker.unlock();
    }

    std::string GetText()
    {
        locker.lock();
        std::stringstream ss("");
        for (uint32_t i = 0; i < stream.size(); ++i)
            ss << stream[i];
        locker.unlock();
        return ss.str();
    }
}