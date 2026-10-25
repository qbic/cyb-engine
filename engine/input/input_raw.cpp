#include "core/logger.h"
#include "core/sys.h"
#include "input/input_raw.h"
#include <array>
#include <windows.h>
#include <hidsdi.h>
#include <malloc.h>

#pragma comment(lib, "Hid.lib")

namespace cyb::input
{
    extern KeyboardState g_keyboard;
    extern MouseState g_mouse;
}

namespace cyb::input::rawinput
{
    LRESULT CALLBACK InputWindowProc(HWND, UINT, WPARAM, LPARAM);

    [[nodiscard]] RAWINPUTDEVICE PickInputDevice(NativeWindowHandle window, USHORT usage) noexcept
    {
        return RAWINPUTDEVICE{
            .usUsagePage = HID_USAGE_PAGE_GENERIC,
            .usUsage = usage,
            .dwFlags = RIDEV_INPUTSINK,
            .hwndTarget = window
        };
    }

    void Init(NativeWindowHandle window) noexcept
    {
        // register mouse (0) and keyboard (1)
        std::array rid = std::to_array<RAWINPUTDEVICE>({
            PickInputDevice(window, HID_USAGE_GENERIC_MOUSE),
            PickInputDevice(window, HID_USAGE_GENERIC_KEYBOARD)
        });

        if (RegisterRawInputDevices(rid.data(), (UINT)rid.size(), sizeof(rid[0])) == FALSE)
            Panicf("RegisterRawInputDevices failed: {}", GetLastError());

        // hook in InputWindowProc to the global window proc
        g_WindowProc = InputWindowProc;
    }

    [[nodiscard]] static uint32_t TranslateKey(int virtualCode)
    {
        switch (virtualCode)
        {
        case VK_UP:
            virtualCode = KEYBOARD_BUTTON_UP;
            break;
        case VK_DOWN:
            virtualCode = KEYBOARD_BUTTON_DOWN;
            break;
        case VK_LEFT:
            virtualCode = KEYBOARD_BUTTON_LEFT;
            break;
        case VK_RIGHT:
            virtualCode = KEYBOARD_BUTTON_RIGHT;
            break;
        case VK_SPACE:
            virtualCode = KEYBOARD_BUTTON_SPACE;
            break;
        case VK_F1:
            virtualCode = KEYBOARD_BUTTON_F1;
            break;
        case VK_F2:
            virtualCode = KEYBOARD_BUTTON_F2;
            break;
        case VK_F3:
            virtualCode = KEYBOARD_BUTTON_F3;
            break;
        case VK_F4:
            virtualCode = KEYBOARD_BUTTON_F4;
            break;
        case VK_F5:
            virtualCode = KEYBOARD_BUTTON_F5;
            break;
        case VK_F6:
            virtualCode = KEYBOARD_BUTTON_F6;
            break;
        case VK_F7:
            virtualCode = KEYBOARD_BUTTON_F7;
            break;
        case VK_F8:
            virtualCode = KEYBOARD_BUTTON_F8;
            break;
        case VK_F9:
            virtualCode = KEYBOARD_BUTTON_F9;
            break;
        case VK_F10:
            virtualCode = KEYBOARD_BUTTON_F10;
            break;
        case VK_F11:
            virtualCode = KEYBOARD_BUTTON_F11;
            break;
        case VK_F12:
            virtualCode = KEYBOARD_BUTTON_F12;
            break;
        case VK_ESCAPE:
            virtualCode = KEYBOARD_BUTTON_ESCAPE;
            break;
        case VK_RETURN:
            virtualCode = KEYBOARD_BUTTON_ENTER;
            break;
        case VK_LSHIFT:
            virtualCode = KEYBOARD_BUTTON_LSHIFT;
            break;
        case VK_RSHIFT:
            virtualCode = KEYBOARD_BUTTON_RSHIFT;
            break;
        }
        return virtualCode;
    }

    static void ParseRawInputBlock(KeyboardState& keyboard, MouseState& mouse, const RAWINPUT& raw)
    {
        if (raw.header.dwType == RIM_TYPEKEYBOARD)
        {
            const RAWKEYBOARD& rawkeyboard = raw.data.keyboard;
            assert(rawkeyboard.VKey < 256);

            // Ignore key 255 as it is sent as a press sequence and is not a real buttons.
            if (rawkeyboard.VKey == 255)
                return;

            //const uint16_t scanCode = rawkeyboard.MakeCode;
            // if((keyboard.Flags & RI_KEY_E1) != 0)
            //    scanCode = ioKeyboard::GetKeyScanCode(keyboard.VKey);
            //bool extended = (rawkeyboard.Flags & RI_KEY_E0) != 0;

            const int key = TranslateKey(rawkeyboard.VKey);
            const bool keyDown = (rawkeyboard.Flags & RI_KEY_BREAK) == 0;
            keyboard.SetKey(key, keyDown);
        }
        else if (raw.header.dwType == RIM_TYPEMOUSE)
        {
            const RAWMOUSE& rawmouse = raw.data.mouse;

            if (rawmouse.usFlags == MOUSE_MOVE_RELATIVE)
            {
                if (std::abs(rawmouse.lLastX) < 30000)
                    mouse.pointerDelta.x += static_cast<float>(rawmouse.lLastX);
                if (std::abs(rawmouse.lLastY) < 30000)
                    mouse.pointerDelta.y += static_cast<float>(rawmouse.lLastY);
                if (rawmouse.usButtonFlags == RI_MOUSE_WHEEL)
                    mouse.wheelDelta += static_cast<float>((SHORT)rawmouse.usButtonData) / static_cast<float>(WHEEL_DELTA);
            }

            if (rawmouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
                SetFlag(mouse.currentButtonState, MouseButton::Left, true);
            else if (rawmouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
                SetFlag(mouse.currentButtonState, MouseButton::Left, false);
            else if (rawmouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
                SetFlag(mouse.currentButtonState, MouseButton::Right, true);
            else if (rawmouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
                SetFlag(mouse.currentButtonState, MouseButton::Right, false);
            else if (rawmouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
                SetFlag(mouse.currentButtonState, MouseButton::Middle, true);
            else if (rawmouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
                SetFlag(mouse.currentButtonState, MouseButton::Middle, false);
        }
    }

    LRESULT CALLBACK InputWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_INPUT:
        {
            UINT dwSize = 0;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            
            std::byte* lpData = (std::byte*)_malloca(dwSize);
            if (lpData == nullptr)
            {
                CYB_WARNING("InputWindowProc: Failed to allocate {} bytes! Dropping input event.", dwSize);
                return 0;
            }

            [[maybe_unused]] UINT dwBytesCopied = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpData, &dwSize, sizeof(RAWINPUTHEADER));
            assert(dwBytesCopied == dwSize);
                
            const RAWINPUT* lpRawInput = (const RAWINPUT*)lpData;
            ParseRawInputBlock(g_keyboard, g_mouse, *lpRawInput);
            _freea(lpData);
        } break;
        }
        
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

} // namespace cyb::input::rawinput