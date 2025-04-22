#version 450
#include "globals.glsl"

layout(binding = 0) uniform sampler2D sampler0;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main()
{
	const float middle = texture(sampler0, uv).r;
    const float outlineThickness = postprocess.param0.y;
	const vec4 outlineColor = postprocess.param1;
	const float outlineThreshold = postprocess.param0.x * middle;
	const vec2 dim = vec2(postprocess.param0.z, postprocess.param0.w);

	vec2 ox = vec2(outlineThickness / dim.x, 0.0);
	vec2 oy = vec2(0.0, outlineThickness / dim.y);
	vec2 PP = uv - oy;

	float CC = texture(sampler0, (PP - ox)).r;	float g00 = (CC);
	CC = texture(sampler0, PP).r;				float g01 = (CC);
	CC = texture(sampler0, PP + ox).r;			float g02 = (CC);
	PP = uv;
	CC = texture(sampler0, PP - ox).r;			float g10 = (CC);
	CC = middle;								float g11 = (CC) * 0.01f;
	CC = texture(sampler0, PP + ox).r;			float g12 = (CC);
	PP = uv + oy;
	CC = texture(sampler0, PP - ox).r;			float g20 = (CC);
	CC = texture(sampler0, PP).r;				float g21 = (CC);
	CC = texture(sampler0, PP + ox).r;			float g22 = (CC);
	float K00 = -1;
	float K01 = -2;
	float K02 = -1;
	float K10 = 0;
	float K11 = 0;
	float K12 = 0;
	float K20 = 1;
	float K21 = 2;
	float K22 = 1;
	float sx = 0;
	float sy = 0;
	sx += g00 * K00;
	sx += g01 * K01;
	sx += g02 * K02;
	sx += g10 * K10;
	sx += g11 * K11;
	sx += g12 * K12;
	sx += g20 * K20;
	sx += g21 * K21;
	sx += g22 * K22;
	sy += g00 * K00;
	sy += g01 * K10;
	sy += g02 * K20;
	sy += g10 * K01;
	sy += g11 * K11;
	sy += g12 * K21;
	sy += g20 * K02;
	sy += g21 * K12;
	sy += g22 * K22;
	float dist = sqrt(sx*sx + sy*sy);

	float edge = dist > outlineThreshold ? 1 : 0;

	color = vec4(outlineColor.rgb, outlineColor.a * dist);
}