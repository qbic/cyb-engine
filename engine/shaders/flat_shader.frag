#version 450
#include "globals.glsl"

layout(location = 0) in FS_IN_DATA
{
    vec3 pos;
    vec4 col;
} fs_in;

layout(location = 0) out vec4 final_color;

void ApplyFog(float dist, inout vec4 color)
{
    const vec3 V = vec3(0.0, 0.0, 0.0);
	color.rgb = mix(color.rgb, GetDynamicSkyColor(V), GetFogAmount(dist));
}

void main() 
{
    final_color = vec4(fs_in.col);

    float dist = length(cbCamera.pos.xyz - fs_in.pos);
    ApplyFog(dist, final_color);
}