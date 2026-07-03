#include <Windows.h>
#include <dwmapi.h> // DwmSetWindowAttribute
#include "cyb-engine.h"
#include "game.h"
#include "resource.h"
#include "../build/config.h"

#include "graphics/window.h"

#pragma comment(lib, "dwmapi.lib")

#define MAX_LOADSTRING 100


extern LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ATOM RegisterWindowClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
std::string GetLastErrorMessage();

GameApplication application;

bool EnterFullscreenMode(uint64_t modeIndex)
{
    auto modeList = cyb::EnumeratePrimaryDisplayVideoModes();
    cyb::VideoMode& mode = modeList[modeIndex];

    DEVMODE fullscreenSettings{};
    fullscreenSettings.dmSize = sizeof(fullscreenSettings);
    EnumDisplaySettings(NULL, 0, &fullscreenSettings);
    fullscreenSettings.dmPelsWidth = mode.width;
    fullscreenSettings.dmPelsHeight = mode.height;
    fullscreenSettings.dmBitsPerPel = 32;
    fullscreenSettings.dmDisplayFrequency = mode.refreshRate;
    fullscreenSettings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

    HWND hwnd = (HWND)application.GetWindow().GetNativeHandle();
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    bool isChangeSuccessful = ChangeDisplaySettings(&fullscreenSettings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
    ShowWindow(hwnd, SW_MAXIMIZE);

    //application.SetWindow(hwnd);

    return isChangeSuccessful;
}

//int WINAPI WinMain(_In_ HINSTANCE hInstance,
//    _In_opt_ HINSTANCE /* hPrevInstance */,
//    _In_ LPSTR /* lpCmdLine */,
//    _In_ int nShowCmd)
//{
//    // load resource strings
//    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
//    LoadStringW(hInstance, IDC_CYBGAME, szWindowClass, MAX_LOADSTRING);
//    LoadStringW(hInstance, IDS_TEXTLOG, szTextlogFile, MAX_LOADSTRING);
//
//    // setup engine logger output modules
//    cyb::RegisterLogOutputModule<cyb::LogOutputModule_VisualStudio>();
//    cyb::RegisterLogOutputModule<cyb::LogOutputModule_File>(szTextlogFile);
//
//    // configure asset search paths
//    cyb::resourcemanager::AddSearchPath("assets/");
//    cyb::resourcemanager::AddSearchPath("../assets/");
//
//
//    application.Init();
//    application.UpdateLoop();
//
//    return 0;
//}
#if 0
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
    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = szWindowClass;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    return RegisterClassW(&wc);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    CONST LONG width = 1920;
    CONST LONG height = 1080;

    RECT rc = { 0, 0, width, height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    std::wstring title = std::wstring(szTitle) + L" v" + cyb::Utf8ToWide(CYB_VERSION_STRING);

    HWND hWnd = CreateWindowW(
        szWindowClass,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        0,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
        return FALSE;

    SendMessage(hWnd, WM_SETTINGCHANGE, 0, 0); // trigger dark mode theme detection
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return hWnd;
}

[[nodiscard]] bool SystemIsUsingDarkMode()
{
    HKEY hKey;
    DWORD value = 1; // Default to light mode (1 = light, 0 = dark)
    DWORD dataSize = sizeof(value);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        RegQueryValueEx(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &dataSize);
        RegCloseKey(hKey);
    }
    return value == 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // if imgui takes an input from the user we need to return
    // so that it doesen't follow though to the game.
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_NCCREATE:
    {
        EnableNonClientDpiScaling(hWnd);
    }
    break;

    case WM_CREATE:
        application.SetWindow(hWnd);
        break;

    case WM_SETTINGCHANGE: [[fallthrough]];
    case WM_DWMCOMPOSITIONCHANGED:
    {
        BOOL darkmode = SystemIsUsingDarkMode();
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkmode, sizeof(darkmode));
    }
    break;

    case WM_SIZE: [[fallthrough]];
    case WM_DPICHANGED:
    {
        if (application.IsWindowActive())
            application.SetWindow(hWnd);
    }
    break;

    case WM_KILLFOCUS:
        application.KillWindowFocus();
        break;
    case WM_SETFOCUS:
        application.SetWindowFocus();
        break;
    case WM_CLOSE: [[fallthrough]];
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

#endif