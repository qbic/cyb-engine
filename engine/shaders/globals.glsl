//? #version 450
#ifndef _GLOBALS_GLSL
#define _GLOBALS_GLSL
#include "shader_interop.h"

#define PI 3.14159265359

#define GAMMA(x)		(ApplySRGBCurve_Fast(x))
#define DEGAMMA(x)		(RemoveSRGBCurve_Fast(x))

#define saturate(x)     (clamp(x, 0.0, 1.0))

// Create vertex position for a full screen triangle from 3 vertices
vec4 CreateFullscreenTriangle(in uint vertexID) {
    const float x = (vertexID / 2) * 4.0 - 1.0;
    const float y = (vertexID % 2) * 4.0 - 1.0;
    return vec4(x, y, 0.0, 1.0);
}

// Create vertex uv for a full screen triangle from 3 vertices
vec2 CreateFullscreenTriangleUV(in uint vertexID) {
	const float u = (vertexID / 2) * 2.0;
    const float v = 1 - (vertexID % 2) * 2.0;	// flip y axis
    return vec2(u, v);
}

vec3 DecodeNormal(const in uint packedBits) {
	vec3 result;
	result.x = float((packedBits >> 0u) & 0xFF) / 255.0 * 2.0 - 1.0;
	result.y = float((packedBits >> 8u) & 0xFF) / 255.0 * 2.0 - 1.0;
	result.z = float((packedBits >> 16u) & 0xFF) / 255.0 * 2.0 - 1.0;
	return result;
}

// Compact, self-contained version of IQ's 3D value noise function.
float Noise_3D(in vec3 p) {
	const vec3 s = vec3(7, 157, 113);
	const vec3 i = floor(p);
    vec4 h = vec4(0.0, s.yz, s.y + s.z) + dot(i, s);
    p -= i; 
    p = p*p*(3.0 - 2.0*p);
    h = mix(fract(sin(h) * 4375.85453), fract(sin(h + s.x) * 4375.85453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z); // Range: [0, 1].
}

vec3 GetDynamicSkyColor(in vec3 V, bool drawSun) {
    float a = V.y * 0.5 + 0.5;
    vec3 color = mix(cbFrame.horizon, cbFrame.zenith, a);

    if (drawSun) {
		vec3 sunDir = normalize(cbFrame.lights[cbFrame.mostImportantLightIndex].position.xyz);
        float sundot = saturate(dot(V, sunDir));
        color += 0.18 * vec3(1.0, 0.7, 0.4) * pow(sundot, 12.0);
        color += 0.18 * vec3(1.0, 0.7, 0.4) * pow(sundot, 32.0);
        color += 0.36 * vec3(1.0, 0.7, 0.4) * pow(sundot, 256.0);
    }

    float t = (cbFrame.cloudHeight - cbCamera.pos.y - 0.15)/(V.y + 0.15);
    if (t > 0.0) {
        vec3 uv = (cbCamera.pos.xyz + t*V) + vec3(vec2(cbFrame.windSpeed * cbFrame.time), 0);
        uv = cbFrame.cloudTurbulence * uv / cbFrame.cloudHeight;
        const float fbm_noise = Noise_3D(uv * 1.0) * 0.73 + 
                                Noise_3D(uv * 2.0) * 0.38 + 
                                Noise_3D(uv * 4.0) * 0.14 + 
                                Noise_3D(uv * 6.0) * 0.06;
        color = mix(color, vec3(2), smoothstep(1.0-cbFrame.cloudiness, 1.0, fbm_noise)*smoothstep(0.45, 0.65, V.y*0.5 + 0.5) * 0.4);
    }

    return color;
}

float GetFogAmount(float dist) {
	return saturate((dist - cbFrame.fog.x) * cbFrame.fog.w);
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