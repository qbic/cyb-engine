//? #version 450
#include "globals.glsl"
#ifndef NO_LIGHTING
#include "lighting.glsl"
#endif

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

layout(location = 0) in GS_IN_DATA
{
    vec3 pos;
    vec4 col;
    vec3 normal;
} gs_in[];

layout(location = 0) out GS_OUT_DATA
{
    vec3 pos;
    vec3 viewDir;
    vec4 color;
} gs_out;

void main() 
{
#ifndef NO_LIGHTING
    const vec3 a = gs_in[0].pos - gs_in[1].pos;
    const vec3 b = gs_in[2].pos - gs_in[1].pos;
    const vec3 normal = normalize(cross(a, b));
#endif

    vec3 vertex_colors[3];
    for (int vertex_index = 0; vertex_index < 3; vertex_index++)
    {
 #ifndef NO_LIGHTING       
        Surface surface = CreateSurface(normal, gs_in[vertex_index].pos, gs_in[vertex_index].col.rgb);
        Lighting lighting = Lighting(vec3(0.0), vec3(0.0));

        for (int light_index = 0; light_index < cbFrame.numLights; light_index++)
        {
            switch (cbFrame.lights[light_index].type)
            {
            case LIGHTSOURCE_TYPE_DIRECTIONAL:
                Light_Directional(cbFrame.lights[light_index], surface, lighting);
                break;
            case LIGHTSOURCE_TYPE_POINT:
                Light_Point(cbFrame.lights[light_index], surface, lighting);
                break;
            }
        }

        ApplyLighting(surface, lighting, vertex_colors[vertex_index]);
#else
        vertex_colors[vertex_index] = gs_in[vertex_index].col.rgb * cbMaterial.baseColor.rgb;
#endif
    }

    // calculate average color of the triangle
    const float alpha = 0.33333 * (gs_in[0].col.a + gs_in[1].col.a + gs_in[2].col.a);
    //vec4 final_color = vec4(vertex_colors[0], alpha);
    vec4 final_color = vec4(0.33333 * (vertex_colors[0] + vertex_colors[1] + vertex_colors[2]), alpha);

    // add a slight sky color tint to the object, giving it the apperance 
    // of some object to sky reflectance (maybe add-in material property)
    const vec3 sky_reflectance = mix(clamp(mix(cbFrame.horizon, cbFrame.zenith, vec3(0.5)) * 1.5, 0, 1), vec3(1.0), 0.8);
    final_color.rgb *= sky_reflectance;

    for (int i = 0; i < gl_in.length(); i++)
    {
        gl_Position = gl_in[i].gl_Position;
        gs_out.pos = gs_in[i].pos;
        gs_out.viewDir = normalize(cbCamera.pos.xyz - gs_in[i].pos);
        gs_out.color = final_color;
        EmitVertex();
    }
    
    EndPrimitive();
}