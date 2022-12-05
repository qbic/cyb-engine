#pragma once
#include "core/core-defs.h"

namespace cyb::input
{
    namespace key
    {
        enum Button
        {
            kNone = 0,

            // Mouse buttons
            MB_LEFT,
            MB_RIGHT2,
            MB_MIDDLE,

            // Keyboard buttons
            KB_UP,
            KB_DOWN,
            KB_LEFT,
            KB_RIGHT,
            KB_SPACE,
            KB_F1,
            KB_F2,
            KB_F3,
            KB_F4,
            KB_F5,
            KB_F6,
            KB_F7,
            KB_F8,
            KB_F9,
            KB_F10,
            KB_F11,
            KB_F12,
            KB_ESCAPE,
            KB_ENTER,
            KB_LSHIFT,      // NOT WORKING
            KB_RSHIFT,      // NOT WORKING

            SPECIAL_KEY_COUNT,

            CHARACHTER_RANGE_START = 'A',       // 'A' == 65
        };

    }

    void Initialize();

    // Call once per frame _AFTER_ user input is processed
    void Update();

    // Check if a button is down
    bool IsDown(uint32_t button);

    // Check if a button is pressed once
    bool WasPressed(uint32_t button);

    // Get pointer position (eg. mouse)
    XMFLOAT2 GetMousePosition();

    // Get pointer delta position from last frame
    XMFLOAT2 GetMousePositionDelta();

    // Set pointer visabliaty
    void ShowMouseCursor(bool value);
};

//static_assert(cyb::input::key::SPECIAL_KEY_COUNT < cyb::input::key::CHARACHTER_RANGE_START);
