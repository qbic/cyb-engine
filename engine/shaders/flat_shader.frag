#version 450
#include "globals.glsl"

layout(location = 0) in PsInput
{
    vec3 position;
    flat vec4 color;
} psIn;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 viewDir = camera.pos.xyz - psIn.position;
    const float dist = length(viewDir);
    viewDir = viewDir / dist;       // normalize
    const vec4 fogColor = vec4(GetDynamicSkyColor(-viewDir, false), 1.0);
    outColor = mix(psIn.color, fogColor, GetFogAmount(dist));
}
