#include "globals.glsl"

struct Lighting
{
    vec3 diffuse;
    vec3 specular;
};

struct Surface
{
    vec3 P;             // world space position
    vec3 N;             // world space normal
    vec3 V;             // world space view vector
    vec3 color;         // diffuse light absorbation value (rgb)
    float roughness;    // roughness: [0:smooth -> 1:rough]
    float metalness;    // metallness: [0 -> 1]
};

struct SurfaceToLight
{
    vec3 L;             // surface to light vector
    float NdotL;        // cos angle between normal and light direction
    vec3 R;
};

Surface CreateSurface(in vec3 N, in vec3 P, in vec3 C)
{
    Surface surface;
    surface.P = P;
    surface.N = N;
    surface.V = camera_cb.pos.xyz;
    surface.color = C * material_cb.base_color.rgb;
    surface.roughness = material_cb.roughness;
    surface.metalness = material_cb.metalness;
    return surface;
}

SurfaceToLight CreateSurfaceToLight(in Surface surface, in vec3 Lnormalized)
{
    SurfaceToLight surface_to_light;
    surface_to_light.L = Lnormalized;
    surface_to_light.NdotL = max(dot(surface_to_light.L, surface.N), 0.0);
    surface_to_light.R = reflect(surface_to_light.L, surface.N);
    return surface_to_light;
}

float PhongBRDF(in Surface surface, in SurfaceToLight surface_to_light) 
{
    const vec3 surface_to_camera = normalize(surface.P - surface.V);
    const float spec_dot = max(dot(surface_to_light.R, surface_to_camera), 0.0);
    const float shininess = mix(1.0, 32.0, 1.0 - surface.roughness);
    const float specular = pow(spec_dot, shininess);
    return specular;
}

void ApplyDirectionalLight(in LightSource light, in Surface surface, inout Lighting lighting)
{
    const vec3 L = normalize(light.position.xyz);

    SurfaceToLight surface_to_light = CreateSurfaceToLight(surface, L);
    
    const vec3 light_color = light.color.rgb * light.energy;
    const float Ka = mix(0.5, 1.0, 1.0 - surface.metalness);

    lighting.diffuse += light_color * Ka * surface_to_light.NdotL;
    lighting.specular += light_color * PhongBRDF(surface, surface_to_light) * surface.metalness;
}

void ApplyPointLight(in LightSource light, in Surface surface, inout Lighting lighting)
{
    vec3 L = light.position.xyz - surface.P;
    const float dist2 = dot(L, L);
    const float range2 = light.range * light.range;

    if (dist2 < range2)
    {
        const float dist = sqrt(dist2);
        L /= dist;

        SurfaceToLight surface_to_light = CreateSurfaceToLight(surface, L);

        const float att = clamp(1.0 - (dist2 / range2), 0.0, 1.0);
        const float attenuation = att * att;
        const vec3 lightColor = light.color.rgb * light.energy * attenuation;
        const float Ka = mix(0.5, 1.0, 1.0 - surface.metalness);

        lighting.diffuse += lightColor * Ka * surface_to_light.NdotL;
        lighting.specular += lightColor * PhongBRDF(surface, surface_to_light) * surface.metalness;;
    }
}

void ApplyLighting(in Surface surface, in Lighting lighting, out vec3 color)
{
    const float abmient = 0.12;
    color = surface.color * abmient;
    color += surface.color * lighting.diffuse;
    color += lighting.specular;
}