#pragma once
#include <string>
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
	namespace LogLevel
	{
		enum : uint16_t
		{
			kTrace,
			kInfo,
			kWarning,
			kError
		};
	}

	class LogOutputModule
	{
	public:
		virtual ~LogOutputModule() = default;
		virtual void Write(const std::string& msg) = 0;
	};

	void RegisterOutputModule(std::shared_ptr<LogOutputModule> output, bool writeHistory = true);

	std::string GetText();

	// Post a message to all registered output modules
	void Post(uint16_t loglevel, const std::string& input);

	template <typename ...T>
	void PostTrace(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::kTrace, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostInfo(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::kInfo, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostWarning(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::kWarning, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostError(fmt::format_string<T...> fmt, T&&... args)
	{
		Post(LogLevel::kError, fmt::format(fmt, std::forward<T>(args)...));
	}
}

#define CYB_TRACE(...)		::cyb::logger::PostTrace(__VA_ARGS__)
#define CYB_INFO(...)		::cyb::logger::PostInfo(__VA_ARGS__)
#define CYB_WARNING(...)	::cyb::logger::PostWarning(__VA_ARGS__)
#define CYB_ERROR(...)		::cyb::logger::PostError(__VA_ARGS__)

