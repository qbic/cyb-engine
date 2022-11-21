#include <Windows.h>
#include "cyb-engine.h"
#include "game.h"

int WINAPI WinMain(HINSTANCE _In_ hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    cyb::platform::Initialize(hInstance);
    cyb::platform::WindowCreateDescription windowDesc;
    windowDesc.title = L"CybEngine Demo";
    windowDesc.size = XMFLOAT2(1920, 1080);
    cyb::platform::CreateMainWindow(windowDesc);
   
    Game application;
    application.SetWindow(cyb::platform::main_window);

    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            application.Run();
        }
    }

	return EXIT_SUCCESS;
}