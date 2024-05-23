#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include "core/platform.h"

#define CYB_LOGLEVEL_TRACE		1
#define CYB_LOGLEVEL_INFO		2
#define CYB_LOGLEVEL_WARNING	3
#define CYB_LOGLEVEL_ERROR		4

// configures the verbosity of the logs
#define	CYB_LOGLEVEL_THRESHOLD	CYB_LOGLEVEL_TRACE

#if CYB_LOGLEVEL_THRESHOLD <= CYB_LOGLEVEL_TRACE
#define CYB_TRACE(...)		cyb::logger::PostTrace(__VA_ARGS__)
#else
#define CYB_TRACE(...)
#endif

#if CYB_LOGLEVEL_THRESHOLD <= CYB_LOGLEVEL_INFO
#define CYB_INFO(...)		cyb::logger::PostInfo(__VA_ARGS__)
#else
#define CYB_INFO(...)
#endif

#if CYB_LOGLEVEL_THRESHOLD <= CYB_LOGLEVEL_WARNING
#define CYB_WARNING(...)	cyb::logger::PostWarning(__VA_ARGS__)
#define CYB_CWARNING(expr, ...) { if (expr) { CYB_WARNING(__VA_ARGS__); }}
#else
#define CYB_WARNING(...)
#define CYB_CWARNING(expr, ...)
#endif

#if CYB_LOGLEVEL_THRESHOLD <= CYB_LOGLEVEL_ERROR
#define CYB_ERROR(...)		cyb::logger::PostError(__VA_ARGS__)
#define CYB_CERROR(expr, ...)	{ if (expr) { CYB_ERROR(__VA_ARGS__); }}
#else
#define CYB_ERROR(...)
#define CYB_CERROR(expr, ...)
#endif

namespace cyb::logger {

	struct Message {
		std::string text;
		std::chrono::system_clock::time_point timestamp;
		uint8_t severity;
	};

	class OutputModule {
	public:
		virtual ~OutputModule() = default;
		virtual void Write(const Message& log) = 0;
	};

	class OutputModule_StringBuffer : public OutputModule {
	public:
		OutputModule_StringBuffer(std::string* output);
		void Write(const logger::Message& log) override;

	private:
		std::stringstream m_logStream;
		std::string* m_stringBuffer;
	};

	class OutputModule_File : public OutputModule {
	public:
		OutputModule_File(const std::filesystem::path& filename, bool timestampMessages = true);
		void Write(const logger::Message& log) override;

	private:
		std::ofstream m_output;
		std::filesystem::path m_filename;
		bool m_useTimestamp;
	};

#ifdef _WIN32
	class LogOutputModule_VisualStudio : public OutputModule {
	public:
		void Write(const cyb::logger::Message& log) override {
			OutputDebugStringA(log.text.c_str());
		}
	};
#endif // _WIN32

	namespace detail {
		void RegisterOutputModule(std::unique_ptr<OutputModule>&& outputModule);
	
		// Prefixes the input text with a log level identifier and 
		// sends a LogMessage to all registered output modules
		void Post(uint8_t severity, const std::string& text);
	}

	template <typename T, typename ...Args,
		typename std::enable_if<
		std::is_base_of<OutputModule, T>{} &&
		std::is_constructible<T, Args&&...>{}, bool>::type = true>
	void RegisterOutputModule(Args&&... args) {
		detail::RegisterOutputModule(std::make_unique<T>(std::forward<Args>(args)...));
	}

	template <typename ...T>
	void PostTrace(fmt::format_string<T...> fmt, T&&... args) {
		detail::Post(CYB_LOGLEVEL_TRACE, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostInfo(fmt::format_string<T...> fmt, T&&... args) {
		detail::Post(CYB_LOGLEVEL_INFO, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostWarning(fmt::format_string<T...> fmt, T&&... args) {
		detail::Post(CYB_LOGLEVEL_WARNING, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename ...T>
	void PostError(fmt::format_string<T...> fmt, T&&... args) {
		detail::Post(CYB_LOGLEVEL_ERROR, fmt::format(fmt, std::forward<T>(args)...));
	}
}