#version 450
#include "globals.glsl"

layout(location = 0) in FS_IN_DATA
{
    vec3 pos;
    vec3 viewDir;
    flat vec4 color;
} fs_in;

layout(location = 0) out vec4 final_color;

void ApplyFog(vec3 V, float dist, inout vec4 color)
{
	color.rgb = mix(color.rgb, GetDynamicSkyColor(V, false), GetFogAmount(dist));
}

void main() 
{
    final_color = vec4(fs_in.color);

    float dist = length(cbCamera.pos.xyz - fs_in.pos);
    ApplyFog(-fs_in.viewDir, dist, final_color);
}