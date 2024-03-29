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

    OutputModule_StringBuffer::OutputModule_StringBuffer(std::string* output) :
        stringBuffer(output)
    {
    }

    void OutputModule_StringBuffer::Write(const logger::Message& log) 
    {
        logStream << log.message;
        if (stringBuffer != nullptr)
        {
            // FIXME: This has bad performance on larger streams
            *stringBuffer = logStream.str();
        }
    }

    OutputModule_File::OutputModule_File(const std::filesystem::path& filename, bool timestampMessages) :
        filename(filename),
        timestampMessages(timestampMessages)
    {
        output.open(filename);
    }

    void OutputModule_File::Write(const logger::Message& log)
    {
        if (output.is_open())
        {
            if (timestampMessages)
            {
                const auto time = std::chrono::system_clock::to_time_t(log.timestamp);
                output << std::put_time(std::localtime(&time), "%Y-%m-%d %X ");
            }

            output << log.message;
        }
    }

    void priv::RegisterOutputModule(std::unique_ptr<OutputModule>&& outputModule)
    {
        for (const auto& it : logHistory)
        {
            outputModule->Write(it);
        }

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