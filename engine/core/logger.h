#pragma once
#include <string>
#include <filesystem>
#include <format>
#include <fstream>
#include <chrono>

#define CYB_TRACE(...)		cyb::PostTrace(__VA_ARGS__)
#define CYB_INFO(...)		cyb::PostInfo(__VA_ARGS__)
#define CYB_WARNING(...)	cyb::PostWarning(__VA_ARGS__)
#define CYB_ERROR(...)		cyb::PostError(__VA_ARGS__)

#define CYB_CWARNING(expr, ...) { if (expr) { CYB_WARNING(__VA_ARGS__); }}
#define CYB_CERROR(expr, ...)	{ if (expr) { CYB_ERROR(__VA_ARGS__); }}
#define CYB_INFO_HR()		CYB_INFO("=======================================================================");

namespace cyb
{
    enum class LogMessageSeverityLevel
    {
        Trace,
        Info,
        Warning,
        Error
    };

    struct LogMessage
    {
        std::string text;
        std::chrono::system_clock::time_point timestamp;
        LogMessageSeverityLevel severity{ LogMessageSeverityLevel::Trace };
    };

    class LogOutputModule
    {
    public:
        virtual ~LogOutputModule() = default;
        virtual void Write(const LogMessage& msg) = 0;
    };

    class LogOutputModule_StringBuffer : public LogOutputModule
    {
    public:
        LogOutputModule_StringBuffer(std::string& output);
        void Write(const LogMessage& msg) override;

    private:
        std::string& m_stringBuffer;
    };

    class LogOutputModule_File : public LogOutputModule
    {
    public:
        LogOutputModule_File(const std::filesystem::path& filename, bool timestampMessages = true);
        void Write(const LogMessage& msg) override;

    private:
        std::ofstream m_file;
        std::filesystem::path m_filename;
        bool m_writeTimestamp;
    };

#ifdef _WIN32
    class LogOutputModule_VisualStudio : public LogOutputModule
    {
    public:
        void Write(const LogMessage& msg) override;
    };
#endif // _WIN32

    namespace detail
    {
        void RegisterLogOutputModule(std::unique_ptr<LogOutputModule>&& outputModule);

        // Prefixes the input text with a log level identifier and 
        // sends a LogMessage to all registered output modules
        void Post(LogMessageSeverityLevel severity, const std::string& text);
    } // namespace detail

    template <typename T, typename ...Args>
    requires std::derived_from<T, LogOutputModule> &&
             std::constructible_from<T, Args...>
    void RegisterLogOutputModule(Args&&... args)
    {
        detail::RegisterLogOutputModule(std::make_unique<T>(std::forward<Args>(args)...));
    }

    template <typename ...T>
    void PostTrace(std::format_string<T...> fmt, T&&... args)
    {
        detail::Post(LogMessageSeverityLevel::Trace, std::format(fmt, std::forward<T>(args)...));
    }

    template <typename ...T>
    void PostInfo(std::format_string<T...> fmt, T&&... args)
    {
        detail::Post(LogMessageSeverityLevel::Info, std::format(fmt, std::forward<T>(args)...));
    }

    template <typename ...T>
    void PostWarning(std::format_string<T...> fmt, T&&... args)
    {
        detail::Post(LogMessageSeverityLevel::Warning, std::format(fmt, std::forward<T>(args)...));
    }

    template <typename ...T>
    void PostError(std::format_string<T...> fmt, T&&... args)
    {
        detail::Post(LogMessageSeverityLevel::Error, std::format(fmt, std::forward<T>(args)...));
    }
} // namespace cyb