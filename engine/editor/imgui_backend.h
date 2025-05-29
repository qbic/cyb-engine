#pragma once

#include "cyb-engine.h"

#define ICON_EYE    "\xee\x81\x9b"      // 0xe05b

void ImGui_Impl_CybEngine_Init(cyb::WindowHandle window);
void ImGui_Impl_CybEngine_Update();
void ImGui_Impl_CybEngine_Compose(cyb::rhi::CommandList cmd);