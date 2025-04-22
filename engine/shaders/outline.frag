#version 450
#include "globals.glsl"

layout(binding = 0) uniform sampler2D sampler0;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main()
{
    const float outlineThickness = postprocess.param0.x;
	const vec4 outlineColor = postprocess.param1;

    const vec2 texelSize = outlineThickness / vec2(textureSize(sampler0, 0));
    const float tl = texture(sampler0, uv + texelSize * vec2(-1,  1)).r; // top-left
    const float tc = texture(sampler0, uv + texelSize * vec2( 0,  1)).r; // top-center
    const float tr = texture(sampler0, uv + texelSize * vec2( 1,  1)).r; // top-right
    const float ml = texture(sampler0, uv + texelSize * vec2(-1,  0)).r; // mid-left
    const float mr = texture(sampler0, uv + texelSize * vec2( 1,  0)).r; // mid-right
    const float bl = texture(sampler0, uv + texelSize * vec2(-1, -1)).r; // bottom-left
    const float bc = texture(sampler0, uv + texelSize * vec2( 0, -1)).r; // bottom-center
    const float br = texture(sampler0, uv + texelSize * vec2( 1, -1)).r; // bottom-right

    // Sobel convolution
    const float Gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    const float Gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;

	const float dist = length(vec2(Gx, Gy));
	color = vec4(outlineColor.rgb, outlineColor.a * dist);
}