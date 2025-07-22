//? #version 450
#include "globals.glsl"

//-----------------------------------------------------------------------------

struct Surface
{
    vec3 P;             // world space position
    vec3 N;             // world space normal
    vec3 V;             // world space view vector
    vec3 baseColor;     // diffuse light absorbation value (rgb)
    float roughness;    // roughness: [0:smooth -> 1:rough]
    float metallic;     // metallic: [0 -> 1]
    float NdotV;        // cos angle between normal view vector
};

Surface CreateSurface(const vec3 N, const vec3 P, const vec3 C)
{
    Surface surface;
    surface.P = P;
    surface.N = N;
    surface.V = normalize(camera.pos.xyz - surface.P);
    surface.baseColor = C * cbMaterial.baseColor.rgb;
    surface.roughness = cbMaterial.roughness;
    surface.metallic = cbMaterial.metalness;
    surface.NdotV = saturate(dot(surface.N, surface.V));
    return surface;
}

//-----------------------------------------------------------------------------

struct SurfaceToLight
{
    vec3 L;             // surface to light vector
    vec3 H;             // half-vector between view vector and light vector
    float NdotL;        // cos angle between normal and light direction
    float NdotH;        // cos angle between normal and half vector
    float LdotH;        // cos angle between light and half vector
};

SurfaceToLight CreateSurfaceToLight(const Surface surface, const vec3 Lnormalized)
{
    SurfaceToLight surfaceToLight;
    surfaceToLight.L     = Lnormalized;
    surfaceToLight.H     = normalize(surfaceToLight.L + surface.V);
    surfaceToLight.NdotL = saturate(dot(surface.N, surfaceToLight.L));
    surfaceToLight.NdotH = saturate(dot(surface.N, surfaceToLight.H));
    surfaceToLight.LdotH = saturate(dot(surfaceToLight.L, surfaceToLight.H));
    return surfaceToLight;
}

//-----------------------------------------------------------------------------

float LambertDiffuse(const Surface surface, const SurfaceToLight surfaceToLight)
{
    const float ambient = 0.25;
    const float f0 = mix(0.5, 1.0, 1.0 - surface.metallic);
    return f0 * mix(ambient, 1.0, surfaceToLight.NdotL);
}

float OrenNayarFastDiffuse(const Surface surface, const SurfaceToLight surfaceToLight)
{
    const float ambient = 0.25;
    
    float LdotV = max(dot(surfaceToLight.L, surface.V), 0.0);
    float sigma2 = surface.roughness * surface.roughness;
    float A = 1.0 + sigma2 * (-0.25 / (sigma2 + 0.13) + 0.5 / (sigma2 + 0.33));
    float B = 0.45 * sigma2 / (sigma2 + 0.09);
    float s = LdotV - surfaceToLight.NdotL * surface.NdotV;
    float t = (s <= 0.0) ? 1.0 : max(surfaceToLight.NdotL, surface.NdotV);
    float orenNayar = surfaceToLight.NdotL * (A + B * s / t);

    const float f0 = mix(0.5, 1.0, 1.0 - surface.metallic);
    return f0 * mix(ambient, 1.0, orenNayar);

}

#ifdef DISNEY_BRDF
float SchlickWeight(in float cosTheta)
{
    const float m = saturate(1.0 - cosTheta);
    const float m2 = m * m;
    return m2 * m2 * m;
}

float DisneyBRDF_Diffuse(in Surface surface, in SurfaceToLight surfaceToLight)
{
    const float FL = SchlickWeight(surfaceToLight.NdotL);
    const float FV = SchlickWeight(surface.NdotV);

    const float Fd90 = 0.5 + 2.0 * surfaceToLight.LdotH * surfaceToLight.LdotH * surface.roughness;
    const float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV) * (1.0 / PI);

    return Fd * 0.75 + 0.25;
}
#endif // DISNEY_BRDF

//-----------------------------------------------------------------------------

float BlinnPhongSpecular(const Surface surface, const SurfaceToLight surfaceToLight)
{
    const float invRoughness = 1.1 - surface.roughness;
    const float shininess = mix(1.0, 64.0, invRoughness);
    return pow(surfaceToLight.NdotH, shininess) * surface.metallic * (invRoughness * 0.6 + 0.4);
}

//-----------------------------------------------------------------------------

struct LightingPart
{
    vec3 diffuse;
    vec3 specular;
};

float GetDiffuseLighting(const Surface surface, const SurfaceToLight surfaceToLight)
{
#ifdef DISNEY_BRDF
    return DisneyBRDF_Diffuse(surface, surfaceToLight);
#else
    //return LambertDiffuse(surface, surfaceToLight);
    return OrenNayarFastDiffuse(surface, surfaceToLight);
#endif
}

float GetSpecularLighting(const Surface surface, const SurfaceToLight surfaceToLight)
{
    return BlinnPhongSpecular(surface, surfaceToLight);
}

LightingPart Light_Directional(const LightSource light, const Surface surface)
{
    const vec3 L = normalize(light.position.xyz);
    const SurfaceToLight surfaceToLight = CreateSurfaceToLight(surface, L);
    const vec3 lightColor = light.color.rgb * light.energy;
    
    return LightingPart(
        lightColor * GetDiffuseLighting(surface, surfaceToLight),
        lightColor * GetSpecularLighting(surface, surfaceToLight)
    );
}

LightingPart Light_Point(const LightSource light, const Surface surface)
{
    const vec3 L = light.position.xyz - surface.P;
    const float dist2 = dot(L, L);
    const float range2 = light.range * light.range;

    // Early-out to skip lights that don't affect surface
    if (dist2 >= range2)
        return LightingPart(vec3(0.0), vec3(0.0));

    const float invDist = inversesqrt(dist2);
    const SurfaceToLight surfaceToLight = CreateSurfaceToLight(surface, L * invDist);
    const float attenuation = saturate(1.0 - (dist2 / range2));
    const vec3 lightColor = light.color.rgb * light.energy * (attenuation * attenuation);

    return LightingPart(
        lightColor * GetDiffuseLighting(surface, surfaceToLight),
        lightColor * GetSpecularLighting(surface, surfaceToLight)
    );
}