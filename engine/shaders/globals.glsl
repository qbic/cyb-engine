//? #version 450
#ifndef _GLOBALS_GLSL
#define _GLOBALS_GLSL
#include "shader_interop.h"

#define PI 3.14159265359

#define GAMMA(x)        (ApplySRGBCurve_Fast(x))
#define DEGAMMA(x)      (RemoveSRGBCurve_Fast(x))

/**
 * @brief Clamps the value to a [0..1] range.
 */
#define saturate(x)     (clamp(x, 0.0, 1.0))

/**
 * @brief Get vertex position for a full screen triangle from 3 indices.
 * @param vertexIndex Vertex index (gl_VertexIndex).
 * @return Vertex position in screenspace.
 */
vec4 GetFullscreenTriangle(const uint vertexIndex)
{
    return vec4((vertexIndex / 2) * 4.0 - 1.0,
                (vertexIndex % 2) * 4.0 - 1.0, 0.0, 1.0);
}

/**
 * @brief Get uv for a full screen triangle from 3 indices.
 * @param vertexIndex Vertex index (gl_VertexIndex).
 * @return Texture coord for the triangle.
 */
vec2 GetFullscreenTriangleUV(const uint vertexIndex)
{
    return vec2((vertexIndex / 2) * 2.0, 1 - (vertexIndex % 2) * 2.0);
}

/**
 * @brief Decode a float packed normal into a normalized vec3.
 */
vec3 DecodePackedNormal(const float bits)
{
    const uint intBits = floatBitsToUint(bits);
    const vec3 result = vec3(
        float((intBits >>  0u) & 0xFF) / 255.0 * 2.0 - 1.0,
        float((intBits >>  8u) & 0xFF) / 255.0 * 2.0 - 1.0,
        float((intBits >> 16u) & 0xFF) / 255.0 * 2.0 - 1.0);
	return result;
}

/**
 * @brief Compact, self-contained version of IQ's 3D value noise function.
 */
float Noise_3D(vec3 p)
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

/**
 * @brief Get the sky color (with sun) at vertex position.
 * 
 * The sun will be drawn at the location of the scene's most
 * dominant directional lightsource.
 * 
 * @param V Normalized worldpos to camera vector.
 * @param drawSun Whether or not to draw the sun.
 * @return RGB color of the sky.
 */
vec3 GetDynamicSkyColor(const vec3 V, bool drawSun)
{
    float a = V.y * 0.5 + 0.5;
    vec3 color = mix(cbFrame.horizon, cbFrame.zenith, a);

    if (drawSun)
    {
           vec3 sunDir = normalize(cbFrame.lights[cbFrame.mostImportantLightIndex].position.xyz);
        float sundot = saturate(dot(V, sunDir));
        color += 0.18 * vec3(1.0, 0.7, 0.4) * pow(sundot, 12.0);
        color += 0.18 * vec3(1.0, 0.7, 0.4) * pow(sundot, 32.0);
        color += 0.36 * vec3(1.0, 0.7, 0.4) * pow(sundot, 256.0);
    }

    float t = (cbFrame.cloudHeight - camera.pos.y - 0.15)/(V.y + 0.15);
    if (t > 0.0)
    {
        vec3 uv = (camera.pos.xyz + t*V) + vec3(vec2(cbFrame.windSpeed * cbFrame.time), 0);
        uv = cbFrame.cloudTurbulence * uv / cbFrame.cloudHeight;
        const float fbm_noise = Noise_3D(uv * 1.0) * 0.73 + 
                                Noise_3D(uv * 2.0) * 0.38 + 
                                Noise_3D(uv * 4.0) * 0.14 + 
                                Noise_3D(uv * 6.0) * 0.06;
        color = mix(color, vec3(2), smoothstep(1.0-cbFrame.cloudiness, 1.0, fbm_noise)*smoothstep(0.45, 0.65, V.y*0.5 + 0.5) * 0.4);
    }

    return color;
}

float GetFogAmount(float dist)
{
    return saturate((dist - cbFrame.fog.x) * cbFrame.fog.w);
}

/**
 * @brief Calculate the normal of 3 vectors.
 * @return The normalized cross product of a, b & c.
 */
vec3 CalcNormal(const vec3 a, const vec3 b, const vec3 c)
{
    return normalize(cross(a - b, c - b));
}

/**
 * @brief Calculate the average value of 3 float.
 */
float CalcAverage(const float a, const float b, const float c)
{
    return (a + b + c) * (1.0 / 3.0);
}

/**
 * @brief Calculate the average value of 3 vec3.
 */
vec3 CalcAverage(const vec3 a, const vec3 b, const vec3 c)
{
    return (a + b + c) * (1.0 / 3.0);
}

/**
 * @brief Calculate the average value of 3 vec3 in an array.
 */
vec3 CalcAverage(const vec3 v[3])
{
    return (v[0] + v[1] + v[2]) * (1.0 / 3.0);
}

#endif