#version 450
#include "globals.glsl"

layout(location = 0) out vec2 uv;

void main()
{
	gl_Position = GetFullscreenTriangle(gl_VertexIndex);
	uv = GetFullscreenTriangleUV(gl_VertexIndex);
}