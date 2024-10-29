//? #version 450
#ifndef _GLOBALS_GLSL
#define _GLOBALS_GLSL
#include "shader_interop.h"

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

vec3 DecodeNormal(const in uint packedBits)
{
	vec3 result;
	result.x = float((packedBits >> 0u) & 0xFF) / 255.0 * 2.0 - 1.0;
	result.y = float((packedBits >> 8u) & 0xFF) / 255.0 * 2.0 - 1.0;
	result.z = float((packedBits >> 16u) & 0xFF) / 255.0 * 2.0 - 1.0;
	return result;
}

// Compact, self-contained version of IQ's 3D value noise function.
float noise(in vec3 p)
{
	const vec3 s = vec3(7, 157, 113);
	const vec3 i = floor(p);
    vec4 h = vec4(0.0, s.yz, s.y + s.z) + dot(i, s);
    p -= i; 
    p = p*p*(3.0 - 2.0*p);
    h = mix(fract(sin(h) * 4375.85453), fract(sin(h + s.x) * 4375.85453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z); // Range: [0, 1].
}

float fbm(in vec3 p)
{
    return noise(p)*.57 + 
           noise(p * 2.0) * 0.28 + 
           noise(p * 4.0) * 0.14 + 
           noise(p * 6.0) * 0.05;
}

vec3 GetDynamicSkyColor(in vec3 V, bool drawSun)
{
    float a = V.y * 0.5 + 0.5;
    vec3 color = mix(cbFrame.horizon, cbFrame.zenith, a);

    if (drawSun)
    {
		vec3 sunDir = normalize(cbFrame.lights[cbFrame.mostImportantLightIndex].position.xyz);
        float sundot = clamp(dot(V, sunDir), 0.0, 1.0);
        color += 0.18 * vec3(1.0, 0.7, 0.4) * pow(sundot, 12.0);
        color += 0.18 * vec3(1.0, 0.7, 0.4) * pow(sundot, 32.0);
        color += 0.36 * vec3(1.0, 0.7, 0.4) * pow(sundot, 256.0);
    }

    float t = (cbFrame.cloudHeight - cbCamera.pos.y - .15)/(V.y + .15);
    if (t > 0.0)
    {
        vec3 uv = (cbCamera.pos.xyz + t*V) + vec3(vec2(cbFrame.windSpeed * cbFrame.time), 0);
        color = mix(color, vec3(2), smoothstep(1.0-cbFrame.cloudiness, 1.0, fbm(cbFrame.cloudTurbulence*uv/cbFrame.cloudHeight))*smoothstep(0.45, 0.65, V.y*0.5 + 0.5) * 0.4);
    }
    return color;
}

float GetFogAmount(float dist)
{
	return clamp((dist - cbFrame.fog.x) / (cbFrame.fog.y - cbFrame.fog.x), 0.0, 1.0);
}

vec3 FaceNormal(in vec3 a, in vec3 b, in vec3 c) {
    const vec3 ab = a - b;
    const vec3 cb = c - b;
    return normalize(cross(ab, cb));
}

float AverageValue(in float a, in float b, in float c) {
    return (a + b + c) * (1.0 / 3.0);
}

vec3 AverageValue(in vec3 a, in vec3 b, in vec3 c) {
    return (a + b + c) * (1.0 / 3.0);
}

vec3 AverageValue(in vec3 v[3]) {
    return (v[0] + v[1] + v[2]) * (1.0 / 3.0);
}

#endif