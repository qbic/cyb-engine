#pragma once
#include "input/input.h"

namespace cyb::input::rawinput
{
    /**
     * @brief Initialize the raw input api for keyboard and mouse.
     */
    void Initialize(WindowHandle window);

    /**
     * @brief Update the current state of keyboard and mouse.
     */
    void Update(KeyboardState& keyboard, MouseState& mouse);

    /**
     * @brief Parse a raw input message from windows message queue.
     * 
     * This Should be called within the WndProc on WM_INPUT, like:
     *  case WM_INPUT:
     *      input::rawinput::ParseMessage((HRAWINPUT)lParam);
     */
    void ParseMessage(HRAWINPUT hRawInput);
}