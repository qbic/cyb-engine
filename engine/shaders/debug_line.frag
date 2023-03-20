#version 450
#include "globals.glsl"

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 final_color;

void main()
{
    final_color = in_color * cbMaterial.baseColor;
}