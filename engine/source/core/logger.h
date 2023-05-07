#pragma once
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#define FMT_HEADER_ONLY
#include <fmt/format.h>

// Enabling CYB_ERRORS_ARE_FATAL will couse CYB_ERROR(...) to teminate the application.
#if CYB_DEBUG_BUILD
#define CYB_ERRORS_ARE_FATAL	0
#else
#define CYB_ERRORS_ARE_FATAL	1
#endif

namespace cyb::logger
{
	enum class Level
	{
		None,
		Trace,
		Info,
		Warning,
		Error
	};

	struct Message
	{
		std::string message;
		std::chrono::system_clock::time_point timestamp;
		Level severity = Level::None;
	};

	class OutputModule
	{
	public:
		virtual ~OutputModule() = default;
		virtual void Write(const Message& log) = 0;
	};

	class OutputModule_StringBuffer : public OutputModule
	{
	public:
		OutputModule_StringBuffer(bool writeTimestamp = false) :
			m_writeTimestamp(writeTimestamp)
		{
		}

		void Write(const logger::Message& log) override
		{
			if (m_writeTimestamp)
			{
				const auto time = std::chrono::system_clock::to_time_t(log.timestamp);
				m_logStream << std::put_time(std::localtime(&time), "%Y-%m-%d %X ");
			}

			m_logStream << log.message;
			m_stringBuffer = m_logStream.str();
		}

		[[nodiscard]] const std::string& GetStringBuffer() const
		{
			return m_stringBuffer;
		}

	private:
		std::stringstream m_logStream;
		std::string m_stringBuffer;
		bool m_writeTimestamp;
	};

	void RegisterOutputModule(std::shared_ptr<OutputModule> output, bool writeHistory = true);

	// Prefixes the input message with a log level identifier and 
	// sends a LogMessage to all registered output modules
	void Post(Level severity, const std::string& input);

	template <typename ...T>
	void PostTrace(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(Level::Trace, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostInfo(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(Level::Info, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostWarning(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(Level::Warning, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostError(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(Level::Error, fmt::format(fmt, std::forward<T>(args)...));
	}
}

#define CYB_TRACE(...)		::cyb::logger::PostTrace(__VA_ARGS__)
#define CYB_INFO(...)		::cyb::logger::PostInfo(__VA_ARGS__)
#define CYB_WARNING(...)	::cyb::logger::PostWarning(__VA_ARGS__)
#define CYB_ERROR(...)		::cyb::logger::PostError(__VA_ARGS__)

#define CYB_CWARNING(expr, ...) { if (expr) { CYB_WARNING(__VA_ARGS__); }}
#define CYB_CERROR(expr, ...)	{ if (expr) { CYB_ERROR(__VA_ARGS__); }}

