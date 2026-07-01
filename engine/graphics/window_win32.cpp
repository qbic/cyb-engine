#include <dwmapi.h>
#include "graphics/display.h"
#include "graphics/window.h"

#pragma comment(lib, "dwmapi.lib")

extern LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace cyb
{
    static constexpr const WCHAR* WND_CLASS_NAME = L"cybWndClass";

    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

    /** Check if system is using dark mode theme. */
    [[nodiscard]] static bool SystemIsUsingDarkMode()
    {
        HKEY hKey;
        DWORD value = 1; // Default to light mode (1 = light, 0 = dark)
        DWORD dataSize = sizeof(value);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &dataSize);
            RegCloseKey(hKey);
        }
        return value == 0;
    }

    [[nodiscard]] static bool RegisterWindowClass(HINSTANCE hInstance)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(hInstance, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = WND_CLASS_NAME;
        wc.hIcon = LoadIconW(hInstance, IDI_HAND);
        return RegisterClassExW(&wc) != 0;
    }

    ClientWindow ClientWindow::Create(const ClientWindowDesc& desc)
    {
        // Set the process DPI awareness to Per Monitor V2
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Register the window class
        HINSTANCE hInstance = GetModuleHandleW(nullptr);
        if (!RegisterWindowClass(hInstance))
            Panicf("Failed to register window class: {}", GetLastError());

        // Create the window at the center of the screen
        DWORD flags = WS_OVERLAPPEDWINDOW;
        if (!desc.resizable)
            flags &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        if (!desc.decorated)
            flags = WS_POPUP;

        RECT rc{ 0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height) };
        AdjustWindowRect(&rc, flags, FALSE);

        const UVec2 desktopSize = DisplayWorkArea();
        const int x = (static_cast<int>(desktopSize.x) - (rc.right - rc.left)) / 2;
        const int y = (static_cast<int>(desktopSize.y) - (rc.bottom - rc.top)) / 2;

        const std::wstring title = Utf8ToWide(desc.title);
        HWND hWnd = CreateWindowExW(
            0,
            WND_CLASS_NAME,
            title.c_str(),
            flags,
            x, y,
            rc.right - rc.left,
            rc.bottom - rc.top,
            nullptr, 
            nullptr,
            hInstance,
            nullptr);
        if (!hWnd)
            Panicf("Failed to create window: {}", GetLastError());

        Handle nativeHandle{};
        nativeHandle.hInstance = hInstance;
        nativeHandle.hWnd = hWnd;

        ClientWindow window(nativeHandle, desc);

        // Store a pointer to ourselves in the HWND so WndProc can reach it.
        // Must happen before any messages are dispatched.
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&window));

        // Force a DWM theme update
        SendMessageW(hWnd, WM_DWMCOMPOSITIONCHANGED, 0, 0);

        // Present the window
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);

        return window;
    }

    ClientWindow::ClientWindow(Handle handle, const ClientWindowDesc& desc) :
        m_nativeHandle(handle),
        m_width(desc.width),
        m_height(desc.height)
    {
    }

    ClientWindow::~ClientWindow()
    {
        if (m_nativeHandle.hWnd)
        {
            SetWindowLongPtrW(static_cast<HWND>(m_nativeHandle.hWnd), GWLP_USERDATA, 0);
            DestroyWindow(static_cast<HWND>(m_nativeHandle.hWnd));
        }
    }

    bool ClientWindow::PollEvents() noexcept
    {
        MSG msg{};
        while (PeekMessageW(&msg, m_nativeHandle.hWnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return IsOpen();
    }

    ClientWindow::ClientWindow(ClientWindow&& other) noexcept :
        m_nativeHandle(other.m_nativeHandle),
        m_width(other.m_width),
        m_height(other.m_height)
    {
        other.m_nativeHandle.hWnd = nullptr;
        other.m_nativeHandle.hInstance = nullptr;

        if (m_nativeHandle.hWnd)
            SetWindowLongPtrW(static_cast<HWND>(m_nativeHandle.hWnd), GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(this));
    }

    ClientWindow& ClientWindow::operator=(ClientWindow&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (m_nativeHandle.hWnd)
            DestroyWindow(static_cast<HWND>(m_nativeHandle.hWnd));

        m_nativeHandle.hWnd = other.m_nativeHandle.hWnd;
        m_nativeHandle.hInstance = other.m_nativeHandle.hInstance;
        m_width = other.m_width;
        m_height = other.m_height;

        other.m_nativeHandle.hWnd = nullptr;
        other.m_nativeHandle.hInstance = nullptr;

        if (m_nativeHandle.hWnd)
            SetWindowLongPtrW(static_cast<HWND>(m_nativeHandle.hWnd), GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(this));

        return *this;
    }

    void ClientWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {

        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED)
            {
                m_isMinimized = true;
            }
            else
            {
                RECT rc{};
                GetClientRect(m_nativeHandle.hWnd, &rc);
                m_width = rc.right - rc.left;
                m_height = rc.bottom - rc.top;
                m_isMinimized = false;
            }
        } break;

        case WM_KILLFOCUS:
            m_isActive = false;
            break;

        case WM_SETFOCUS:
            m_isActive = true;
            break;

        case WM_CLOSE:
            m_isOpen = false;
            break;
        }
    }

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // During CreateWindowExW, before GWLP_USERDATA is set,
        // some messages (WM_CREATE, WM_NCCREATE…) arrive with a null pointer,
        // so don't assume a valid pointer!
        auto* window = reinterpret_cast<ClientWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (window)
            window->HandleMessage(msg, wParam, lParam);

        // If imgui takes an input from the user we need to return
        // so that it doesen't follow though to the game.
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        // Handle some (possibly) early window events excplicitly here.
        switch (msg)
        {
        case WM_NCCREATE:
            EnableNonClientDpiScaling(hWnd);
            break;

        case WM_DWMCOMPOSITIONCHANGED:
        {
            CONST BOOL darkmode = SystemIsUsingDarkMode();
            DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkmode, sizeof(darkmode));
        } break;
        }

        return g_WindowProc(hWnd, msg, wParam, lParam);
    }

} // namespace cyb