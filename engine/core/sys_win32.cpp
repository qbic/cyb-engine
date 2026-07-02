#include "core/sys.h"
#include "core/logger.h"

namespace cyb
{
    LRESULT(CALLBACK* g_WindowProc)(HWND, UINT, WPARAM, LPARAM) = DefWindowProcW;

    void Panic(const std::string& message)
    {
        CYB_ERROR("Panic: {}", message);
        CYB_DEBUGBREAK();
        MessageBoxW(GetActiveWindow(), Utf8ToWide(message).c_str(), L"Panic", MB_ICONERROR | MB_OK);
        ExitProcess(-1);
    }

    void Exit(int code)
    {
        CYB_INFO("Exiting application with code {}", code);
        PostQuitMessage(code);
    }

    std::wstring Utf8ToWide(const std::string_view s)
    {
        if (s.empty())
            return {};

        const int size = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            s.data(), (int)s.size(),
            nullptr, 0);

        std::wstring result(size, L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            s.data(), (int)s.size(),
            result.data(), size);

        return result;
    }

    std::string WideToUtf8(const std::wstring_view w)
    {
        if (w.empty())
            return {};

        const int size = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), (int)w.size(),
            nullptr, 0,
            nullptr, nullptr);

        std::string result(size, '\0');
        WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), (int)w.size(),
            result.data(), size,
            nullptr, nullptr);

        return result;
    }
} // namespace cyb
