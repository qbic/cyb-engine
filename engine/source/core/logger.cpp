#include "core/platform.h"
#include "core/spinlock.h"
#include "core/logger.h"
#include <sstream>
#include <deque>
#include <list>
#include <typeinfo>
#include <typeindex>
#include <stdio.h>
#include <stdarg.h>

namespace cyb::logger
{
    std::list<std::shared_ptr<LogOutputModule>> outputModules;
    std::deque<std::string> stream;
    SpinLock locker;
    uint32_t logLevelThreshold = LogLevel::kTrace;

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

    void Post(uint16_t loglevel, const std::string& input)
    {
        if (loglevel < logLevelThreshold)
            return;

        locker.lock();

        std::stringstream ss("");
        switch (loglevel)
        {
        case LogLevel::kTrace: ss << "[TRACE] "; break;
        case LogLevel::kInfo: ss << "[INFO] "; break;
        case LogLevel::kWarning: ss << "[WARNING] "; break;
        case LogLevel::kError: ss << "[ERROR] "; break;
        default: break;
        }

        ss << input << std::endl;
        stream.push_back(ss.str());

        for (auto& output : outputModules)
            output->Write(ss.str());

        locker.unlock();

#if CYB_ERRORS_ARE_FATAL
        if (loglevel == LogLevel::kError)
        {
            platform::CreateMessageWindow(input, "CybEngine Error");
            platform::Exit(1);
        }
#endif
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