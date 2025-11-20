#pragma once
#include <string>
#include <format>
#include <cstdint>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#define CYB_DEBUGBREAK() __debugbreak()
#endif // _WIN32

#define BIT(n)			(1ULL << (n))

namespace cyb
{
    /**
     * @brief Causes a panic in the application with the given message.
     * Panic message will be logged and then the application will be terminated.
     */
    void Panic(const std::string& message);

    /**
     * @brief \see Panic
     */
    template <typename... Args>
    void Panicf(std::format_string<Args...> fmt, Args&&... args)
    {
        Panic(std::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Tries to exit the application gracefully.
     * For windows, this will post a WM_QUIT message.
     */
    void Exit(int code = 0);
} // namespace cyb