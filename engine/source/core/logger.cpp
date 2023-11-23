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
    std::vector<std::unique_ptr<OutputModule>> outputModules;
    std::deque<Message> logHistory;
    SpinLock postLock;
    Level logLevelThreshold = Level::Trace;

    void priv::RegisterOutputModule(std::unique_ptr<OutputModule>&& outputModule)
    {
        outputModules.push_back(std::move(outputModule));
    }

    // Check if the log level is under the global log level threshold.
    inline bool IsUnderLevelThreshold(Level level)
    {
        return static_cast<std::underlying_type<Level>::type>(level) <
            static_cast<std::underlying_type<Level>::type>(logLevelThreshold);
    }

    inline std::string GetLogLevelPrefix(Level severity)
    {
        switch (severity)
        {
        case Level::Trace:   return "[TRACE]";
        case Level::Info:    return "[INFO]";
        case Level::Warning: return "[WARNING]";
        case Level::Error:   return "[ERROR]";
        }

        return {};
    }

    void Post(Level severity, const std::string& input)
    {
        if (IsUnderLevelThreshold(severity))
            return;
        std::scoped_lock<SpinLock> lock(postLock);
        
        Message log;
        log.message = fmt::format("{0} {1}\n", GetLogLevelPrefix(severity), input);
        log.timestamp = std::chrono::system_clock::now();
        log.severity = severity;
        logHistory.push_back(log);

        for (auto& output : outputModules)
            output->Write(log);

#if CYB_ERRORS_ARE_FATAL
        if (severity == Level::Error)
        {
            platform::CreateMessageWindow(input, "CybEngine Error");
            platform::Exit(1);
        }
#endif
    }
}