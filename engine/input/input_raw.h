#pragma once
#include "input/input.h"

namespace cyb::input::rawinput
{
    /** Initialize the raw input api for keyboard and mouse. */
    void Init(NativeWindowHandle window) noexcept;
}