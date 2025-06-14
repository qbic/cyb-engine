#pragma once
#include <array>
#include "core/platform.h"

namespace cyb::input
{
    enum Button
    {
        BUTTON_NONE = 0,

        MOUSE_BUTTON_LEFT,
        MOUSE_BUTTON_RIGHT,
        MOUSE_BUTTON_MIDDLE,

        KEYBOARD_BUTTON_UP,
        KEYBOARD_BUTTON_DOWN,
        KEYBOARD_BUTTON_LEFT,
        KEYBOARD_BUTTON_RIGHT,
        KEYBOARD_BUTTON_SPACE,
        KEYBOARD_BUTTON_F1,
        KEYBOARD_BUTTON_F2,
        KEYBOARD_BUTTON_F3,
        KEYBOARD_BUTTON_F4,
        KEYBOARD_BUTTON_F5,
        KEYBOARD_BUTTON_F6,
        KEYBOARD_BUTTON_F7,
        KEYBOARD_BUTTON_F8,
        KEYBOARD_BUTTON_F9,
        KEYBOARD_BUTTON_F10,
        KEYBOARD_BUTTON_F11,
        KEYBOARD_BUTTON_F12,
        KEYBOARD_BUTTON_ESCAPE,
        KEYBOARD_BUTTON_ENTER,
        KEYBOARD_BUTTON_LSHIFT,
        KEYBOARD_BUTTON_RSHIFT,

        SPECIAL_KEY_COUNT,

        CHARACHTER_RANGE_START = 'A',       // 'A' == 65
    };

    struct ButtonState
    {
        bool isDown = false;
        uint32_t halfTransitionCount = 0;

        void RegisterKeyDown();
        void RegisterKeyUp();
        void Reset();
    };

    struct KeyboardState
    {
        std::array<ButtonState, 256> buttons = {};
    };

    struct MouseState
    {
        XMFLOAT2 position = XMFLOAT2(0, 0);
        XMFLOAT2 deltaPosition = XMFLOAT2(0, 0);
        float deltaWheel = 0;
        ButtonState leftButton = {};
        ButtonState middleButton = {};
        ButtonState rightButton = {};
    };

    // Initialize the input system
    void Initialize();

    // Call once per frame (before processing new input events)
    void Update(WindowHandle window);

    [[nodiscard]] const KeyboardState& GetKeyboardState();
    [[nodiscard]] const MouseState& GetMouseState();

    // Check if a button is down
    [[nodiscard]] bool IsDown(uint32_t button);

    // Check if a button is pressed once
    [[nodiscard]] bool WasPressed(uint32_t button);
}
