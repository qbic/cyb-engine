#version 450
#include "globals.glsl"

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;

layout(location = 0) out VS_OUT_DATA
{
    vec3 pos;
    flat vec4 col;
    vec3 normal;
} vs_out;

void main() 
{
    vec4 pos = vec4(in_position.xyz, 1.0);
    gl_Position = pos * g_xTransform;
    vs_out.pos = (pos * g_xModelMatrix).xyz;
    vs_out.col = in_color;
    vs_out.normal = normalize(UnpackNormal(floatBitsToUint(in_position.w)) * mat3(g_xModelMatrix));
}