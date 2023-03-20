//? #version 450
#ifndef _GLOBALS_GLSL
#define _GLOBALS_GLSL
#include "shader-interop.h"

#define PI 3.14159265359

#define GAMMA(x)		(ApplySRGBCurve_Fast(x))
#define DEGAMMA(x)		(RemoveSRGBCurve_Fast(x))

vec3 ApplySRGBCurve(vec3 x)
{
	// Approximately pow(x, 1.0 / 2.2)
	return all(lessThan(x, vec3(0.0031308))) ? 12.92 * x : 1.055 * pow(x, vec3(1.0 / 2.4)) - 0.055;
}

vec3 RemoveSRGBCurve(vec3 x)
{
    // Approximately pow(x, 2.2)
    return all(lessThan(x, vec3(0.04045))) ? x / 12.92 : pow((x + 0.055) / 1.055, vec3(2.4));
}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
vec3 ApplySRGBCurve_Fast(vec3 x)
{
    return all(lessThan(x, vec3(0.0031308))) ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
}

vec3 RemoveSRGBCurve_Fast(vec3 x)
{
    return all(lessThan(x, vec3(0.04045))) ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

// Creates a full screen triangle from 3 vertices:
void CreateFullscreenTriangle(uint vertexID, out vec4 pos)
{
	pos.x = (vertexID / 2) * 4.0 - 1.0;
	pos.y = (vertexID % 2) * 4.0 - 1.0;
	pos.z = 0.0;
	pos.w = 1.0;
}

void CreateFullscreenTriangleUV(uint vertexID, out vec4 pos, out vec2 uv)
{
	CreateFullscreenTriangle(vertexID, pos);

	uv.x = (vertexID / 2) * 2.0;
	uv.y = ((vertexID+1) % 2) * 2.0;	// Flip Y
}

vec3 UnpackNormal(in uint value)
{
	vec3 result;
	result.x = float((value >> 0u) & 0xFF) / 255.0 * 2.0 - 1.0;
	result.y = float((value >> 8u) & 0xFF) / 255.0 * 2.0 - 1.0;
	result.z = float((value >> 16u) & 0xFF) / 255.0 * 2.0 - 1.0;
	return result;
}

vec3 GetDynamicSkyColor(in vec3 V)
{
    float a = V.y * 0.5 + 0.5;
    return mix(cbFrame.horizon, cbFrame.zenith, a);
}

float GetFogAmount(float dist)
{
	return clamp((dist - cbFrame.fog.x) / (cbFrame.fog.y - cbFrame.fog.x), 0.0, 1.0);
}

#endif