#version 450
#include "globals.glsl"

layout(location=0) in FS_IN_DATA
{
    vec4 position;
    vec2 clipspace;
} fs_in;

layout(location=0) out vec4 final_color;

void main()
{
    vec4 unprojected = camera_cb.inv_vp * vec4(fs_in.clipspace.x, fs_in.clipspace.y, 1.0, 1.0);
	unprojected.xyz /= unprojected.w;

	vec3 V = normalize(unprojected.xyz - camera_cb.pos.xyz);
    final_color = vec4(GetDynamicSkyColor(V), 1.0);
}
