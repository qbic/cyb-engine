#pragma once
#include <string>
#include <filesystem>
#include <fstream>
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
		OutputModule_StringBuffer(std::string* output);
		void Write(const logger::Message& log) override;

	private:
		std::stringstream m_logStream;
		std::string* m_stringBuffer;
	};

	class OutputModule_File : public OutputModule
	{
	public:
		OutputModule_File(const std::filesystem::path& filename, bool timestampMessages = true);
		void Write(const logger::Message& log) override;

	private:
		std::ofstream output;
		std::filesystem::path filename;
		bool timestampMessages;
	};

	namespace priv
	{
		void RegisterOutputModule(std::unique_ptr<OutputModule>&& outputModule);
	}

	template <typename T, typename ...Args>
	void RegisterOutputModule(Args&&... args)
	{
		static_assert(std::is_base_of<cyb::logger::OutputModule, T>::value, "T must be derived from cyb::logger::OutputModule");
		priv::RegisterOutputModule(std::make_unique<T>(std::forward<Args>(args)...));
	}

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

#define CYB_TRACE(...)		cyb::logger::PostTrace(__VA_ARGS__)
#define CYB_INFO(...)		cyb::logger::PostInfo(__VA_ARGS__)
#define CYB_WARNING(...)	cyb::logger::PostWarning(__VA_ARGS__)
#define CYB_ERROR(...)		cyb::logger::PostError(__VA_ARGS__)

#define CYB_CWARNING(expr, ...) { if (expr) { CYB_WARNING(__VA_ARGS__); }}
#define CYB_CERROR(expr, ...)	{ if (expr) { CYB_ERROR(__VA_ARGS__); }}

