#include <deque>
#include <mutex>
#include "core/logger.h"
#include "core/spinlock.h"

// history is only used to append logs to newly registrated output
// modules and should't need to be very big
#define MAX_HISTORY_SIZE     40

namespace cyb::logger {

    Level severityThreshold = Level::Trace;
    std::vector<std::unique_ptr<OutputModule>> outputModules;
    std::deque<Message> logHistory;
    SpinLock postLock;

    OutputModule_StringBuffer::OutputModule_StringBuffer(std::string* output) :
        m_stringBuffer(output) {
    }

    // FIXME: we're copying the whole stringstream on each write, this will
    //        lead to some bad performace when the stringstream gets big
    void OutputModule_StringBuffer::Write(const logger::Message& log) {
        m_logStream << log.text;
        if (m_stringBuffer != nullptr)
            *m_stringBuffer = m_logStream.str();
    }

    OutputModule_File::OutputModule_File(const std::filesystem::path& filename, bool timestampMessages) :
        m_filename(filename),
        m_useTimestamp(timestampMessages) {
        m_output.open(filename);
    }

    void OutputModule_File::Write(const logger::Message& log) {
        if (!m_output.is_open())
            return;

        if (m_useTimestamp) {
            const auto time = std::chrono::system_clock::to_time_t(log.timestamp);
            m_output << std::put_time(std::localtime(&time), "%Y-%m-%d %X ");
        }

        m_output << log.text;
    }

    void SetMessageSeverityThreshold(Level severity) {
        severityThreshold = severity;
    }

    namespace detail {
        void RegisterOutputModule(std::unique_ptr<OutputModule>&& outputModule) {
            std::scoped_lock<SpinLock> lock(postLock);
            for (const auto& it : logHistory) {
                outputModule->Write(it);
            }

            outputModules.push_back(std::move(outputModule));
        }

        [[nodiscard]] static inline std::string GetLogLevelPrefix(Level severity) {
            switch (severity) {
            case Level::Trace:   return "[TRACE]";
            case Level::Info:    return "[INFO]";
            case Level::Warning: return "[WARNING]";
            case Level::Error:   return "[ERROR]";
            }

            return {};
        }

        void Post(Level severity, const std::string& text) {
            if ((int)severity < (int)severityThreshold)
                return;

            Message log;
            log.text = std::format("{0} {1}\n", GetLogLevelPrefix(severity), text);
            log.timestamp = std::chrono::system_clock::now();
            log.severity = severity;

            std::scoped_lock<SpinLock> lock(postLock);

            logHistory.push_back(log);
            while (logHistory.size() > MAX_HISTORY_SIZE)
                logHistory.pop_front();

            for (auto& output : outputModules)
                output->Write(log);
        }
    }
}