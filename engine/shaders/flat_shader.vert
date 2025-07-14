#version 450
#include "globals.glsl"

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out GsInput
{
    vec3 position;
    flat vec4 color;
    vec3 normal;
} vsOut;

void main() 
{
    vec4 pos = vec4(inPosition.xyz, 1.0);
    vsOut.position = (pos * g_xModelMatrix).xyz;
    vsOut.color = inColor;
    vsOut.normal = FloatBitsToNormalizedVec3(inPosition.w) * mat3(g_xModelMatrix);
    gl_Position = pos * g_xTransform;
}