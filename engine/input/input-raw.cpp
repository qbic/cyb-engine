#include "core/arena.h"
#include "core/cvar.h"
#include "core/logger.h"
#include "input/input.h"
#include "input/input-raw.h"
#include <hidsdi.h>
#include <list>
#pragma comment(lib, "Hid.lib")

namespace cyb::input::rawinput
{
    CVar<uint32_t> inputArenaSize("inputArenaSize", 16 * 1024, CVarFlag::SystemBit, "Memory arena size (64bit aligned) for the raw input system (restart requierd)");
    ArenaAllocator inputArena;
    std::vector<RAWINPUT*> inputMessages;

    input::KeyboardState keyboard;
    input::MouseState mouse;
    std::atomic<bool> initialized = false;

    void Initialize()
    {
        std::array<RAWINPUTDEVICE, 2> rid = {};

        // Register mouse:
        rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        rid[0].dwFlags = 0;
        rid[0].hwndTarget = 0;

        // Register keyboard:
        rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
        rid[1].dwFlags = 0;
        rid[1].hwndTarget = 0;

        if (RegisterRawInputDevices(rid.data(), (UINT)rid.size(), sizeof(rid[0])) == FALSE)
        {
            // registration failed. Call GetLastError for the cause of the error
            assert(0);
        }

        inputArena.SetBlockSizeAndAlignment(inputArenaSize.GetValue(), 8);
        inputMessages.reserve(64);
        initialized.store(true);
    }

    void ParseRawInputBlock(const RAWINPUT& raw)
    {
        if (raw.header.dwType == RIM_TYPEKEYBOARD)
        {
            const RAWKEYBOARD& rawkeyboard = raw.data.keyboard;
            assert(rawkeyboard.VKey < keyboard.buttons.size());

            // Ignore key 255 as it is sent as a press sequence and is not a real buttons.
            if (rawkeyboard.VKey == 255)
                return;

            if ((rawkeyboard.Flags & RI_KEY_BREAK) == 0)
                keyboard.buttons[rawkeyboard.VKey].RegisterKeyDown();
            else
                keyboard.buttons[rawkeyboard.VKey].RegisterKeyUp();
        }
        else if (raw.header.dwType == RIM_TYPEMOUSE)
        {
            const RAWMOUSE& rawmouse = raw.data.mouse;

            if (rawmouse.usFlags == MOUSE_MOVE_RELATIVE)
            {
                if (std::abs(rawmouse.lLastX) < 30000)
                    mouse.deltaPosition.x += (float)rawmouse.lLastX;
                if (std::abs(rawmouse.lLastY) < 3000)
                    mouse.deltaPosition.y += (float)rawmouse.lLastY;
                if (rawmouse.usButtonFlags == RI_MOUSE_WHEEL)
                    mouse.deltaWheel += float((SHORT)rawmouse.usButtonData) / float(WHEEL_DELTA);
            }
            else if (rawmouse.usFlags == MOUSE_MOVE_ABSOLUTE)
            {
                // for some reason we never get absolute coordinates with raw input...
            }

            if (rawmouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_DOWN)
                mouse.leftButton.RegisterKeyDown();
            else if (rawmouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_UP)
                mouse.leftButton.RegisterKeyUp();
            else if (rawmouse.usButtonFlags == RI_MOUSE_MIDDLE_BUTTON_DOWN)
                mouse.middleButton.RegisterKeyDown();
            else if (rawmouse.usButtonFlags == RI_MOUSE_MIDDLE_BUTTON_UP)
                mouse.middleButton.RegisterKeyUp();
            else if (rawmouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_DOWN)
                mouse.rightButton.RegisterKeyDown();
            else if (rawmouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_UP)
                mouse.rightButton.RegisterKeyUp();
        }
    }

    void Update(bool hasWindowFocus)
    {
        if (hasWindowFocus)
        {
            for (auto& button : keyboard.buttons)
                button.Reset();

            mouse.position = XMFLOAT2(0, 0);
            mouse.deltaPosition = XMFLOAT2(0, 0);
            mouse.deltaWheel = 0;
            mouse.leftButton.Reset();
            mouse.middleButton.Reset();
            mouse.rightButton.Reset();
        }
        else
        {
            keyboard = input::KeyboardState();
            mouse = input::MouseState();
        }

        // Loop through inputs that we got from text loop
        for (auto& input : inputMessages)
            ParseRawInputBlock(*input);

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

    void GetKeyboardState(input::KeyboardState* state)
    {
        *state = keyboard;
    }

    void GetMouseState(input::MouseState* state)
    {
        *state = mouse;
    }
}