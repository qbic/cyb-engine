#pragma once
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#define FMT_HEADER_ONLY
#include <fmt/format.h>

// This will couse CYB_ERROR to teminate the application
#if CYB_DEBUG_BUILD
#define CYB_ERRORS_ARE_FATAL	0
#else
#define CYB_ERRORS_ARE_FATAL	1
#endif

namespace cyb::logger 
{
	enum class LogLevel
	{
		Trace,
		Info,
		Warning,
		Error
	};

	struct LogMessage
	{
		std::string message;
		std::chrono::system_clock::time_point timestamp;
		LogLevel severity;
	};

	class LogOutputModule
	{
	public:
		virtual ~LogOutputModule() = default;
		virtual void Write(const LogMessage& log) = 0;
	};

	class LogOutputModule_StringBuffer : public LogOutputModule
	{
	public:
		LogOutputModule_StringBuffer(bool writeTimestamp = false) :
			m_writeTimestamp(writeTimestamp)
		{
		}

		void Write(const logger::LogMessage& log) override
		{
			if (m_writeTimestamp)
			{
				const auto time = std::chrono::system_clock::to_time_t(log.timestamp);
				m_logStream << std::put_time(std::localtime(&time), "%Y-%m-%d %X ");
			}

			m_logStream << log.message;
			m_stringBuffer = m_logStream.str();
		}

		const std::string& GetStringBuffer() const
		{
			return m_stringBuffer;
		}

	private:
		std::stringstream m_logStream;
		std::string m_stringBuffer;
		bool m_writeTimestamp;
	};

	void RegisterOutputModule(std::shared_ptr<LogOutputModule> output, bool writeHistory = true);

	// Prefixes the input message with a log level identifier and 
	// sends a LogMessage to all registered output modules
	void Post(LogLevel severity, const std::string& input);

	template <typename ...T>
	void PostTrace(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::Trace, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostInfo(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::Info, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostWarning(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::Warning, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostError(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::Error, fmt::format(fmt, std::forward<T>(args)...));
	}
}

#define CYB_TRACE(...)		::cyb::logger::PostTrace(__VA_ARGS__)
#define CYB_INFO(...)		::cyb::logger::PostInfo(__VA_ARGS__)
#define CYB_WARNING(...)	::cyb::logger::PostWarning(__VA_ARGS__)
#define CYB_ERROR(...)		::cyb::logger::PostError(__VA_ARGS__)

#define CYB_CWARNING(expr, ...) { if (expr) { CYB_WARNING(__VA_ARGS__); }}
#define CYB_CERROR(expr, ...)	{ if (expr) { CYB_ERROR(__VA_ARGS__); }}

