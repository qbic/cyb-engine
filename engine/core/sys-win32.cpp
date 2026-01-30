#include "core/sys.h"
#include "core/logger.h"

namespace cyb
{
    void Panic(const std::string& message)
    {
        CYB_ERROR("Panic: {}", message);
        CYB_DEBUGBREAK();
        MessageBoxA(GetActiveWindow(), message.c_str(), "Panic", MB_OK);
        ExitProcess(-1);
    }

    void Exit(int code)
    {
        CYB_INFO("Exiting application with code {}", code);
        PostQuitMessage(code);
    }

    std::wstring Utf8ToWide(const std::string& s)
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

    std::string WideToUtf8(const std::wstring& w)
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
            result.data(), size, nullptr, nullptr);

        return result;
    }

}