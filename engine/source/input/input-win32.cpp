#include "core/platform.h"
#include "input/input.h"
#include <Windows.h>

namespace cyb::input
{
    static const int KEY_COUNT = 256;

    bool keyDown[KEY_COUNT] = { 0 };
    int keyHalfTransitionCount[KEY_COUNT] = {};
    int key_remap_table[key::SPECIAL_KEY_COUNT] = { 0 };
    XMFLOAT2 mousePos;
    XMFLOAT2 mousePosPrev;

    static_assert(_countof(keyDown) == _countof(keyHalfTransitionCount), "Array size missmatch");

    void Initialize()
    {
        key_remap_table[key::MB_LEFT] = VK_LBUTTON;
        key_remap_table[key::MB_RIGHT2] = VK_RBUTTON;
        key_remap_table[key::MB_MIDDLE] = VK_MBUTTON;
        key_remap_table[key::KB_UP] = VK_UP;
        key_remap_table[key::KB_DOWN] = VK_DOWN;
        key_remap_table[key::KB_LEFT] = VK_LEFT;
        key_remap_table[key::KB_RIGHT] = VK_RIGHT;
        key_remap_table[key::KB_SPACE] = VK_SPACE;
        key_remap_table[key::KB_F1] = VK_F1;
        key_remap_table[key::KB_F2] = VK_F2;
        key_remap_table[key::KB_F3] = VK_F3;
        key_remap_table[key::KB_F4] = VK_F4;
        key_remap_table[key::KB_F5] = VK_F5;
        key_remap_table[key::KB_F6] = VK_F6;
        key_remap_table[key::KB_F7] = VK_F7;
        key_remap_table[key::KB_F8] = VK_F8;
        key_remap_table[key::KB_F9] = VK_F9;
        key_remap_table[key::KB_F10] = VK_F10;
        key_remap_table[key::KB_F11] = VK_F11;
        key_remap_table[key::KB_F12] = VK_F12;
        key_remap_table[key::KB_ESCAPE] = VK_ESCAPE;
        key_remap_table[key::KB_ENTER] = VK_RETURN;
        key_remap_table[key::KB_LSHIFT] = VK_LSHIFT;
        key_remap_table[key::KB_RSHIFT] = VK_RSHIFT;
    }

    void Update(platform::WindowType window)
    {
        // Reset half transition count for all keys
        memset(keyHalfTransitionCount, 0, sizeof(keyHalfTransitionCount));

        // Update mouse position
        mousePosPrev = mousePos;

        POINT p;
        GetCursorPos(&p);
        ScreenToClient(window, &p);
        mousePos = XMFLOAT2((float)p.x, (float)p.y);
    }

    void ProcessKeyboardMessage(int keyIndex, bool isDown)
    {
        assert(keyIndex < _countof(keyDown));

        if (keyDown[keyIndex] != isDown)
        {
            keyDown[keyIndex] = isDown;
            ++keyHalfTransitionCount[keyIndex];
        }
    }

    static uint32_t GetMappedButtonIndex(uint32_t button)
    {
        if (button < key::SPECIAL_KEY_COUNT)
        {
            return key_remap_table[button];
        }

        return button;
    }

    bool IsDown(uint32_t button)
    {
        //if (!platform::main_window->IsActive())
        //{
        //    return false;
        //}

        uint32_t buttonIndex = GetMappedButtonIndex(button);
        assert(buttonIndex < _countof(keyDown));

        return keyDown[buttonIndex];
    }

    bool WasPressed(uint32_t button)
    {
        uint32_t buttonIndex = GetMappedButtonIndex(button);
        assert(buttonIndex < _countof(keyDown));

        bool result = ((keyHalfTransitionCount[buttonIndex] > 1) || (keyHalfTransitionCount[buttonIndex] == 1 && keyDown[buttonIndex]));
        return result;
    }

    XMFLOAT2 GetMousePosition()
    {
        return mousePos;
    }

    XMFLOAT2 GetMousePositionDelta()
    {
        XMFLOAT2 delta;
        delta.x = mousePos.x - mousePosPrev.x;
        delta.y = mousePos.y - mousePosPrev.y;
        return delta;
    }

    void ShowMouseCursor(bool value)
    {
        ::ShowCursor(value);
    }
};

static UINT Win32_MapMouseButton(UINT code)
{
    if (code == WM_LBUTTONDOWN || code == WM_LBUTTONUP)
        return VK_LBUTTON;
    else if (code == WM_RBUTTONDOWN || code == WM_RBUTTONUP)
        return VK_RBUTTON;
    else if (code == WM_MBUTTONDOWN || code == WM_MBUTTONUP)
        return VK_MBUTTON;

    return 0;
}

LRESULT CALLBACK Win32_InputProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        uint32_t VKCode = (uint32_t)wParam;

        //bool AltKeyWasDown = (msg.lParam & (1 << 29));
        //bool ShiftKeyWasDown = (GetKeyState(VK_SHIFT) & (1 << 15));

        // NOTE: Since we are comparing wasDown to isDown, 
        // we MUST use == and != to convert these bit tests to actual
        // 0 or 1 values.
        bool wasDown = ((lParam & (1 << 30)) != 0);
        bool isDown = ((lParam & (1UL << 31)) == 0);

        if (wasDown != isDown)
        {
            cyb::input::ProcessKeyboardMessage(VKCode, isDown);
        }
    } break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    {
        UINT VKCode = Win32_MapMouseButton(uMsg);
        cyb::input::ProcessKeyboardMessage(VKCode, true);
        ::SetCapture(hWnd);
    } break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        UINT VKCode = Win32_MapMouseButton(uMsg);
        cyb::input::ProcessKeyboardMessage(VKCode, false);
        ::ReleaseCapture();
    } break;

    default: break;
    }

    return false;
}
