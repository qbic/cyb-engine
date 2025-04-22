#version 450
#include "globals.glsl"

layout(location = 0) out vec2 uv;

void main()
{
	gl_Position = CreateFullscreenTriangle(gl_VertexIndex);
	uv = CreateFullscreenTriangleUV(gl_VertexIndex);
}