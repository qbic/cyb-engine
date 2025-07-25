#include <deque>
#include <cassert>
#include "core/cvar.h"
#include "core/mutex.h"
#include "core/logger.h"

namespace cyb::logger
{
    // logHistorySize only used to append logs to newly registrated output
    // modules and should't need to be very big
    CVar<uint32_t> logHistorySize("logHistorySize", 40, CVarFlag::SystemBit, "Maximum numbers of log lines to save");
    CVar<uint32_t> logSeverityThreshold("logSeverityThreshold", 0, CVarFlag::SystemBit, "0=Trace, 1=Info, 2=Warning, 3=Error");

    std::vector<std::unique_ptr<OutputModule>> outputModules;
    std::deque<Message> logHistory;
    SpinLock locker;

    OutputModule_StringBuffer::OutputModule_StringBuffer(std::string& output) :
        m_stringBuffer(output)
    {
    }

    void OutputModule_StringBuffer::Write(const logger::Message& msg)
    {
        m_stringBuffer.append(msg.text);
    }

    OutputModule_File::OutputModule_File(const std::filesystem::path& filename, bool timestampMessages) :
        m_filename(filename),
        m_useTimestamp(timestampMessages)
    {
        m_output.open(filename);
    }

    void OutputModule_File::Write(const logger::Message& msg)
    {
        if (!m_output.is_open())
            return;

        if (m_useTimestamp)
        {
            const auto time = std::chrono::system_clock::to_time_t(msg.timestamp);
            m_output << std::put_time(std::localtime(&time), "%Y-%m-%d %X ");
        }

        m_output << msg.text;
    }

#ifdef _WIN32
    void LogOutputModule_VisualStudio::Write(const cyb::logger::Message& msg)
    {
        OutputDebugStringA(msg.text.c_str());
    }
#endif // _WIN32

    namespace detail
    {
        void RegisterOutputModule(std::unique_ptr<OutputModule>&& outputModule)
        {
            ScopedMutex lock(locker);
            for (const auto& it : logHistory)
                outputModule->Write(it);

            outputModules.push_back(std::move(outputModule));
        }

        [[nodiscard]] static inline std::string GetLogLevelPrefix(Level severity)
        {
            switch (severity)
            {
            case Level::Trace:   return "[TRACE]";
            case Level::Info:    return "[INFO]";
            case Level::Warning: return "[WARNING]";
            case Level::Error:   return "[ERROR]";
            }

            assert(0);
            return {};
        }

        void Post(Level severity, const std::string& text)
        {
            if ((uint32_t)severity < logSeverityThreshold.GetValue())
                return;

            Message log;
            log.text = std::format("{0} {1}\n", GetLogLevelPrefix(severity), text);
            log.timestamp = std::chrono::system_clock::now();
            log.severity = severity;

            ScopedMutex lock(locker);
            logHistory.push_back(log);
            while (logHistory.size() > logHistorySize.GetValue())
                logHistory.pop_front();

            for (auto& output : outputModules)
                output->Write(log);
        }
    }
}