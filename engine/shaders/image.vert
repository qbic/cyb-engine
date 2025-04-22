#version 450
#include "globals.glsl"

layout(location = 0) out vec2 frag_uv;

void main()
{
    if ((image.flags & IMAGE_FULLSCREEN_BIT) == IMAGE_FULLSCREEN_BIT)
	{
		gl_Position = CreateFullscreenTriangle(gl_VertexIndex);
		frag_uv = CreateFullscreenTriangleUV(gl_VertexIndex);
    }
	else
	{
		// This vertex shader generates a trianglestrip like this:
		//	1--2
		//	  /
		//	 /
		//	3--4
		gl_Position = image.corners[gl_VertexIndex];
		frag_uv.x = gl_VertexIndex % 2;
		frag_uv.y = 1 - (gl_VertexIndex / 2);
    }
}