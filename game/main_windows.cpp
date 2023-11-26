#include <Windows.h>
#include "cyb-engine.h"
#include "game.h"
#include "resource.h"

#define MAX_LOADSTRING 100

WCHAR szTitle[MAX_LOADSTRING]; 
WCHAR szWindowClass[MAX_LOADSTRING];

extern LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ATOM RegisterWindowClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
std::string GetLastErrorMessage();

Game application;

int WINAPI WinMain(_In_ HINSTANCE hInstance, 
    _In_opt_ HINSTANCE hPrevInstance, 
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    // Setup logger output modules
    cyb::logger::RegisterOutputModule<cyb::logger::LogOutputModule_VisualStudio>();
    cyb::logger::RegisterOutputModule<cyb::logger::OutputModule_File>("textlog.txt");

    BOOL dpiSuccess = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    assert(dpiSuccess);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CYBGAME, szWindowClass, MAX_LOADSTRING);

    RegisterWindowClass(hInstance);
    if (!InitInstance(hInstance, nShowCmd))
    {
        CYB_ERROR("Failed to initialize instance: {}", GetLastErrorMessage());
        return FALSE;
    }

    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else 
        {
            application.Run();
        }
    }

    return (int)msg.wParam;
}

std::string GetLastErrorMessage()
{
    LPSTR s = 0;
    const DWORD code = GetLastError();
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&s, 0, NULL);
    assert(s);
    std::string str(s);
    LocalFree(s);
    return str;
}

ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.lpszClassName = szWindowClass;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    return RegisterClass(&wc);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    DWORD style = WS_POPUP | WS_OVERLAPPED | WS_SYSMENU | WS_BORDER | WS_CAPTION;
    DWORD exStyle = WS_EX_APPWINDOW;

    // TODO: Minimize crashes the engine, so keep it disabled until fixed
    //style |= WS_MINIMIZEBOX;  

    HWND hWnd = CreateWindowEx(
        exStyle,
        szWindowClass,
        szTitle,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1920,
        1080,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    application.SetWindow(hWnd);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        // If ImGUI takes an input from the user we need to return
        // so that it doesen't follow though to the game.
        return true;
    }

    switch (message)
    {
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_INPUT:
        cyb::input::rawinput::ParseMessage((HRAWINPUT)lParam);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC hDC = BeginPaint(hWnd, &paint);
        EndPaint(hWnd, &paint);
    } break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}