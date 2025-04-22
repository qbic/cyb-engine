#version 450
#include "globals.glsl"

layout(location = 0) in FS_IN_DATA {
    vec3 pos;
    flat vec4 color;
} fs_in;

layout(location = 0) out vec4 final_color;

void main() {
    vec3 viewDir = camera.pos.xyz - fs_in.pos;
    const float dist = length(viewDir);
    viewDir = viewDir / dist;       // normalize
    const vec4 fogColor = vec4(GetDynamicSkyColor(-viewDir, false), 1.0);
    final_color = mix(fs_in.color, fogColor, GetFogAmount(dist));
}