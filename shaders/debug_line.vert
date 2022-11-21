#version 450
#include "globals.glsl"

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;

void main()
{
    gl_Position = in_position * g_xTransform;
    out_color = in_color;
}