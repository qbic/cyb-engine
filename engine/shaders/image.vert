#version 450
#include "globals.glsl"

layout(location = 0) out vec2 frag_uv;

void main() {
    if ((cbImage.flags & IMAGE_FULLSCREEN_BIT) != IMAGE_FULLSCREEN_BIT) {
        CreateFullscreenTriangleUV(gl_VertexIndex, gl_Position, frag_uv);
    } else {
		// This vertex shader generates a trianglestrip like this:
		//	1--2
		//	  /
		//	 /
		//	3--4
		gl_Position = cbImage.corners[gl_VertexIndex];
		frag_uv.x = gl_VertexIndex % 2;
		frag_uv.y = gl_VertexIndex / 2;
    }
}