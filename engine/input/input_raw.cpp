#include <array>
#include <windows.h>
#include <hidsdi.h>
#include "core/arena.h"
#include "core/logger.h"
#include "input/input_raw.h"

#pragma comment(lib, "Hid.lib")

namespace cyb::input::rawinput
{
    static constexpr size_t inputArenaBlockSize = 1024 * 2;
    ArenaAllocator inputArena;
    std::vector<RAWINPUT*> inputMessages;
    std::atomic<bool> initialized{ false };

    void Initialize(WindowHandle window)
    {
        std::array<RAWINPUTDEVICE, 2> rid{};

        // Register mouse:
        rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        rid[0].dwFlags = RIDEV_INPUTSINK;
        rid[0].hwndTarget = window;

        // Register keyboard:
        rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
        rid[1].dwFlags = RIDEV_INPUTSINK;
        rid[1].hwndTarget = window;

        if (RegisterRawInputDevices(rid.data(), (UINT)rid.size(), sizeof(rid[0])) == FALSE)
        {
            // registration failed. Call GetLastError for the cause of the error
            assert(0);
        }

        inputArena.SetPageSizeAndAlignment(inputArenaBlockSize, 8);
        inputMessages.reserve(64);
        initialized.store(true);
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

    void Update(KeyboardState& keyboard, MouseState& mouse)
    {
        // Reset mouse delta position
        mouse.pointerDelta.x = 0.0f;
        mouse.pointerDelta.y = 0.0f;

        // Loop through inputs that we got from text loop
        for (auto& input : inputMessages)
            ParseRawInputBlock(keyboard, mouse, *input);

        inputMessages.clear();
        inputArena.Reset();
    }

    void ParseMessage(HRAWINPUT hRawInput)
    {
        if (!initialized.load())
            return;

        UINT dwSize = 0u;

        GetRawInputData(hRawInput, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
        BYTE* lpb = inputArena.Allocate(dwSize);
        if (lpb == nullptr)
        {
            CYB_WARNING("Input message queue full, dropping input data");
            return;
        }

        UINT result = GetRawInputData(hRawInput, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
        if (result == dwSize)
            inputMessages.push_back((RAWINPUT*)lpb);
    }
}