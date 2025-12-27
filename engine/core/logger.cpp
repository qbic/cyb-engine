#include <deque>
#include <cassert>
#include <mutex>
#include "core/cvar.h"
#include "core/spinlock.h"
#include "core/sys.h"
#include "core/logger.h"

namespace cyb
{
    // logHistorySize only used to append logs to newly registered output
    // modules and should't need to be very big
    CVar<uint32_t> logHistorySize{ "logHistorySize", 40, CVarFlag::SystemBit, "Maximum numbers of log lines to save" };
    CVar<uint32_t> logSeverityThreshold{ "logSeverityThreshold", 0, CVarFlag::SystemBit, "0=Trace, 1=Info, 2=Warning, 3=Error" };

    static std::vector<std::unique_ptr<LogOutputModule>> g_outputModules;
    static std::deque<LogMessage> g_logHistory;
    static SpinLock g_locker;

    LogOutputModule_StringBuffer::LogOutputModule_StringBuffer(std::string& output) :
        m_stringBuffer(output)
    {
    }

    void LogOutputModule_StringBuffer::Write(const LogMessage& msg)
    {
        m_stringBuffer.append(msg.text);
    }

    LogOutputModule_File::LogOutputModule_File(const std::filesystem::path& filename, bool timestampMessages) :
        m_filename(filename),
        m_writeTimestamp(timestampMessages)
    {
        m_file.open(filename);
    }

    void LogOutputModule_File::Write(const LogMessage& msg)
    {
        if (!m_file.is_open())
            return;

        if (m_writeTimestamp)
        {
            const auto time = std::chrono::system_clock::to_time_t(msg.timestamp);
            m_file << std::put_time(std::localtime(&time), "%Y-%m-%d %X ");
        }

        m_file << msg.text;
    }

#ifdef _WIN32
    void LogOutputModule_VisualStudio::Write(const cyb::LogMessage& msg)
    {
        OutputDebugStringA(msg.text.c_str());
    }
#endif // _WIN32

    namespace detail
    {
        void RegisterLogOutputModule(std::unique_ptr<LogOutputModule>&& outputModule)
        {
            std::scoped_lock lock{ g_locker };
            for (const auto& it : g_logHistory)
                outputModule->Write(it);

            g_outputModules.push_back(std::move(outputModule));
        }

        [[nodiscard]] static inline std::string GetLogLevelPrefix(LogMessageSeverityLevel severity)
        {
            switch (severity)
            {
            case LogMessageSeverityLevel::Trace:   return "[TRACE]";
            case LogMessageSeverityLevel::Info:    return "[INFO]";
            case LogMessageSeverityLevel::Warning: return "[WARNING]";
            case LogMessageSeverityLevel::Error:   return "[ERROR]";
            }

            assert(0);
            return {};
        }

        void Post(LogMessageSeverityLevel severity, const std::string& text)
        {
            if ((uint32_t)severity < logSeverityThreshold.GetValue())
                return;

            LogMessage log;
            log.text = std::format("{0} {1}\n", GetLogLevelPrefix(severity), text);
            log.timestamp = std::chrono::system_clock::now();
            log.severity = severity;

            std::scoped_lock lock{ g_locker };
            g_logHistory.push_back(log);
            while (g_logHistory.size() > logHistorySize.GetValue())
                g_logHistory.pop_front();

            for (auto& output : g_outputModules)
                output->Write(log);
        }
    } // namespace detail
} // namespace cyb

