#include "core/sys.h"
#include "core/logger.h"

namespace cyb
{
    void Panic(const std::string& message)
    {
        CYB_ERROR("Panic: {}", message);
        MessageBoxA(GetActiveWindow(), message.c_str(), "Panic", MB_OK);
        ExitProcess(-1);
    }

    void Exit(int code)
    {
        CYB_INFO("Exiting application with code {}", code);
        PostQuitMessage(code);
    }
}