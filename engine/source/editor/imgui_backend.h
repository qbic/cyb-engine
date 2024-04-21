#pragma once

#include "cyb-engine.h"

#define ICON_EYE    "\xee\x81\x9b"      // 0xe05b

namespace cyb::editor
{
    // https://github.com/traverseda/OpenFontIcons
    enum Icons
    {
        INTERNAL_ICON_START = 0xe000,


        INTERNAL_ICON_END = 0xe0fe
    };
}

void ImGui_Impl_CybEngine_Init(cyb::platform::WindowType window);
void ImGui_Impl_CybEngine_Update();
void ImGui_Impl_CybEngine_Compose(cyb::graphics::CommandList cmd);