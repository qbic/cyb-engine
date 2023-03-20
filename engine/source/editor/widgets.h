#pragma once
#include <string>
#include <fmt/format.h>
#include "imgui/imgui.h"

namespace cyb::ui
{
    void ItemLabel(const std::string& title, bool isLeft = true);
}