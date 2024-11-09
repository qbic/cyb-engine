//? #version 450
#include "globals.glsl"
#ifndef NO_LIGHTING
#include "lighting.glsl"
#endif

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

layout(location = 0) in GS_IN_DATA {
    vec3 pos;
    vec4 col;
    vec3 normal;
} gs_in[];

layout(location = 0) out GS_OUT_DATA {
    vec3 pos;
    vec3 viewDir;
    flat vec4 color;
} gs_out;

void main()  {
#ifndef NO_LIGHTING
#ifdef COMPUTE_HARD_NORMALS
    const vec3 normal = FaceNormal(gs_in[0].pos, gs_in[1].pos, gs_in[2].pos);
#else
    const vec3 normal = AverageValue(gs_in[0].normal, gs_in[1].normal, gs_in[2].normal);
#endif // COMPUTE_HARD_NORMALS
#endif // NO_LIGHTING

    const vec4 faceColor = mix(gs_in[0].col, gs_in[1].col, float(gs_in[1].col == gs_in[2].col));

    #ifdef ONE_VERTEX_LIGHTING
    vec3 vertex_colors[1];
    const int vertex_index = 0;
    const vec3 vertex_pos = AverageValue(gs_in[0].pos, gs_in[1].pos, gs_in[2].pos);
    #else
    vec3 vertex_colors[3];
    for (int vertex_index = 0; vertex_index < 3; vertex_index++)
    #endif  // ONE_VERTEX_LIGHTING
    {
#ifndef NO_LIGHTING
#ifndef ONE_VERTEX_LIGHTING
        const vec3 vertex_pos = gs_in[vertex_index].pos;
#endif // ONE_VERTEX_LIGHTING
        const Surface surface = CreateSurface(normal, vertex_pos, faceColor.rgb);
        LightingPart lighting = LightingPart(vec3(0.0), vec3(0.0));

        for (int light_index = 0; light_index < cbFrame.numLights; light_index++) {
            switch (cbFrame.lights[light_index].type) {
            case LIGHTSOURCE_TYPE_DIRECTIONAL:
                Light_Directional(cbFrame.lights[light_index], surface, lighting);
                break;
            case LIGHTSOURCE_TYPE_POINT:
                Light_Point(cbFrame.lights[light_index], surface, lighting);
                break;
            }
        }

        vertex_colors[vertex_index] = GetSurfaceLighting(surface, lighting);
#else
        vertex_colors[vertex_index] = faceColor.rgb * cbMaterial.baseColor.rgb;
#endif  // NO_LIGHTING
    }

    // add a slight sky color tint to the object, giving it the apperance 
    // of some object to sky reflectance (maybe add-in material property)
    const vec3 avg_sky_color = (cbFrame.horizon + cbFrame.zenith) * 0.75;
    const vec3 sky_reflectance = mix(saturate(avg_sky_color), vec3(1.0), 0.75);

    // calculate average color of the triangle
    #ifdef ONE_VERTEX_LIGHTING
    const vec3 final_rgb = vertex_colors[0] * sky_reflectance;
    const vec4 final_color = vec4(final_rgb, faceColor.a);
    #else
    const float alpha = AverageValue(gs_in[0].col.a, gs_in[1].col.a, gs_in[2].col.a);
    const vec4 final_color = vec4(AverageValue(vertex_colors), alpha);
    #endif // ONE_VERTEX_LIGHTING

    for (int i = 0; i < gl_in.length(); i++) {
        gl_Position = gl_in[i].gl_Position;
        gs_out.pos = gs_in[i].pos;
        gs_out.viewDir = normalize(cbCamera.pos.xyz - gs_in[i].pos);
        gs_out.color = final_color;
        EmitVertex();
    }
    
    EndPrimitive();
}