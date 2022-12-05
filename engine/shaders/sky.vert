#version 450
#include "globals.glsl"

layout(location=0) in vec3 in_position;

layout(location=0) out VS_OUT_DATA
{
    vec4 position;
    vec2 clipspace;
} vs_out;

void main()
{
    CreateFullscreenTriangle(gl_VertexIndex, vs_out.position);
    vs_out.clipspace = vs_out.position.xy;
    gl_Position = vs_out.position;
}