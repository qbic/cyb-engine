#version 450
#include "globals.glsl"

layout(location = 0) in FS_IN_DATA {
    vec3 pos;
    vec3 viewDir;
    flat vec4 color;
} fs_in;

layout(location = 0) out vec4 final_color;

void main() {
    const float dist = length(cbCamera.pos.xyz - fs_in.pos);
    const vec4 fogColor = vec4(GetDynamicSkyColor(-fs_in.viewDir, false), 1.0);
    final_color = mix(fs_in.color, fogColor, GetFogAmount(dist));
}