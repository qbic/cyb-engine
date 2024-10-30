//? #version 450
#include "globals.glsl"

struct Surface
{
    vec3 P;             // world space position
    vec3 N;             // world space normal
    vec3 V;             // world space view vector
    vec3 baseColor;     // diffuse light absorbation value (rgb)
    float roughness;    // roughness: [0:smooth -> 1:rough]
    float metallic;     // metallic: [0 -> 1]
};

Surface CreateSurface(in vec3 N, in vec3 P, in vec3 C)
{
    Surface surface;
    surface.P = P;
    surface.N = N;
    surface.V = normalize(cbCamera.pos.xyz - surface.P);
    surface.baseColor = C * cbMaterial.baseColor.rgb;
    surface.roughness = cbMaterial.roughness;
    surface.metallic = cbMaterial.metalness;

    return surface;
}

struct SurfaceToLight
{
    vec3 L;             // surface to light vector
    vec3 H;		        // half-vector between view vector and light vector
    float NdotL;        // cos angle between normal and light direction
    float NdotH;        // cos angle between normal and half vector
    float NdotV;
    float LdotH;
};

SurfaceToLight CreateSurfaceToLight(in Surface surface, in vec3 Lnormalized)
{
    SurfaceToLight surfaceToLight;
    surfaceToLight.L = Lnormalized;
    surfaceToLight.H = normalize(surfaceToLight.L + surface.V);
    surfaceToLight.NdotL = saturate(dot(surface.N, surfaceToLight.L));
    surfaceToLight.NdotH = saturate(dot(surface.N, surfaceToLight.H));
    surfaceToLight.NdotV = saturate(dot(surface.N, surface.V));
    surfaceToLight.LdotH = saturate(dot(surfaceToLight.L, surfaceToLight.H));
    return surfaceToLight;
}

float BRDF_GetDiffuse(in Surface surface, in SurfaceToLight surfaceToLight)
{
    // 3/4 Lambert diffuse shading
    const float f0 = mix(0.5, 1.0, 1.0 - surface.metallic);
    return f0 * (surfaceToLight.NdotL * 0.75 + 0.25);
}

float SchlickWeight(float cosTheta) {
    const float m = saturate(1.0 - cosTheta);
    return (m * m) * (m * m) * m;
}

float DisneyDiffuse(in Surface surface, in SurfaceToLight surfaceToLight) {
    const float FL = SchlickWeight(surfaceToLight.NdotL);
    const float FV = SchlickWeight(surfaceToLight.NdotV);
    
    const float Fd90 = 0.5 + 2.0 * surfaceToLight.LdotH*surfaceToLight.LdotH * surface.roughness;
    const float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);
    
    return (1.0/PI) * Fd * 0.75 + 0.25;
}

float BRDF_GetSpecular(in Surface surface, in SurfaceToLight surfaceToLight)
{
    // Blinn-phong specular adjusted for metallic/roughness workflow
    const float spec = surfaceToLight.NdotH;
    const float invRoughness = 1.1 - surface.roughness;
    const float shininess = mix(1.0, 64.0, invRoughness);
    return pow(spec, shininess) * surface.metallic * (invRoughness * 0.6 + 0.4);
}

struct Lighting
{
    vec3 diffuse;
    vec3 specular;
};

float Diffuse_Lighting(in Surface surface, in SurfaceToLight surfaceToLight)
{
#ifdef DISNEY_BRDF
    return DisneyDiffuse(surface, surfaceToLight);
#else
    return BRDF_GetDiffuse(surface, surfaceToLight);
#endif
}

float Specular_Lighting(in Surface surface, in SurfaceToLight surfaceToLight)
{
    return BRDF_GetSpecular(surface, surfaceToLight);
}

void Light_Directional(in LightSource light, in Surface surface, inout Lighting lighting)
{
    const vec3 L = normalize(light.position.xyz);
    SurfaceToLight surfaceToLight = CreateSurfaceToLight(surface, L);
    
    const vec3 lightColor = light.color.rgb * light.energy;
    lighting.diffuse += lightColor * Diffuse_Lighting(surface, surfaceToLight);
    lighting.specular += lightColor * Specular_Lighting(surface, surfaceToLight);
}

void Light_Point(in LightSource light, in Surface surface, inout Lighting lighting)
{
    const vec3 L = light.position.xyz - surface.P;
    const float dist2 = dot(L, L);
    const float range2 = light.range * light.range;

    if (dist2 < range2)
    {
        const float dist = sqrt(dist2);
        SurfaceToLight surfaceToLight = CreateSurfaceToLight(surface, L / dist);

        const float attenuation = saturate(1.0 - (dist2 / range2));
        const vec3 lightColor = light.color.rgb * light.energy * (attenuation * attenuation);
        lighting.diffuse += lightColor * Diffuse_Lighting(surface, surfaceToLight);
        lighting.specular += lightColor * Specular_Lighting(surface, surfaceToLight);
    }
}

void ApplyLighting(in Surface surface, in Lighting lighting, out vec3 color)
{
    const float abmient = 0.00;
    color = (abmient + lighting.diffuse) * surface.baseColor;
    color += lighting.specular;
}