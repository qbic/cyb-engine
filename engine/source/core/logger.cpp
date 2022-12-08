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
    std::deque<LogMessage> logHistory;
    SpinLock locker;
    LogLevel logLevelThreshold = LogLevel::Trace;

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
            for (const auto& log : logHistory)
            {
                output->Write(log);
            }
        }
    }

    // Check if the log level is under the global log level threshold
    inline bool IsUnderLogLevelThreshold(LogLevel level)
    {
        return static_cast<std::underlying_type<LogLevel>::type>(level) <
            static_cast<std::underlying_type<LogLevel>::type>(logLevelThreshold);
    }

    inline std::string GetLogLevelPrefix(LogLevel severity)
    {
        switch (severity)
        {
        case LogLevel::Trace:   return "[TRACE] ";
        case LogLevel::Info:    return "[INFO] ";
        case LogLevel::Warning: return "[WARNING] ";
        case LogLevel::Error:   return "[ERROR] ";
        }

        return "";
    }

    void Post(LogLevel severity, const std::string& input)
    {
        if (IsUnderLogLevelThreshold(severity))
            return;

        locker.lock();
        
        LogMessage log;
        log.message = fmt::format("{0}{1}\n", GetLogLevelPrefix(severity), input);
        log.severity = severity;
        logHistory.push_back(log);

        for (auto& output : outputModules)
            output->Write(log);

        locker.unlock();

#if CYB_ERRORS_ARE_FATAL
        if (severity == LogLevel::Error)
        {
            platform::CreateMessageWindow(input, "CybEngine Error");
            platform::Exit(1);
        }
#endif
    }
}