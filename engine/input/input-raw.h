#pragma once
#include "input/input.h"

namespace cyb::input::rawinput
{
    void Initialize();
    void Update(bool hasWindowFocus);
    void ParseMessage(HRAWINPUT hRawInput);
    void GetKeyboardState(input::KeyboardState* state);
    void GetMouseState(input::MouseState* state);
}