//? #version 450
#include "globals.glsl"
#ifndef NO_LIGHTING
#include "lighting.glsl"
#endif

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

layout(location = 0) in GsInput
{
    vec3 position;
    vec4 color;
    vec3 normal;
} gsIn[];

layout(location = 0) out PsInput
{
    vec3 position;
    flat vec4 color;
} gsOut;

void main() 
{
#ifndef NO_LIGHTING
#ifdef COMPUTE_HARD_NORMALS
    const vec3 normal = FaceNormal(gsIn[0].position, gsIn[1].position, gsIn[2].position);
#else
    const vec3 normal = AverageValue(gsIn[0].normal, gsIn[1].normal, gsIn[2].normal);
#endif // COMPUTE_HARD_NORMALS
#endif // NO_LIGHTING

    const vec4 faceColor = mix(gsIn[0].color, gsIn[1].color, float(gsIn[1].color == gsIn[2].color));

    #ifdef ONE_VERTEX_LIGHTING
    vec3 vertex_colors[1];
    const int vertex_index = 0;
    const vec3 vertex_pos = AverageValue(gsIn[0].position, gsIn[1].position, gsIn[2].position);
    #else
    vec3 vertex_colors[3];
    for (int vertex_index = 0; vertex_index < 3; vertex_index++)
    #endif  // ONE_VERTEX_LIGHTING
    {
#ifndef NO_LIGHTING
#ifndef ONE_VERTEX_LIGHTING
        const vec3 vertex_pos = gsIn[vertex_index].position;
#endif // ONE_VERTEX_LIGHTING
        const Surface surface = CreateSurface(normal, vertex_pos, faceColor.rgb);
        LightingPart lighting = LightingPart(vec3(0.0), vec3(0.0));

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

        vertex_colors[vertex_index] = GetSurfaceLighting(surface, lighting);
#else
        vertex_colors[vertex_index] = faceColor.rgb * cbMaterial.baseColor.rgb;
#endif  // NO_LIGHTING
    }

    // add a slight sky color tint to the object, giving it the apperance 
    // of some object to sky reflectance (maybe add-in material property)
    const vec3 avg_sky_color = (cbFrame.horizon + cbFrame.zenith) * 0.75;
    const vec3 sky_reflectance = mix(saturate(avg_sky_color), vec3(1.0), 0.85);

    // calculate average color of the triangle
    #ifdef ONE_VERTEX_LIGHTING
    const vec3 final_rgb = vertex_colors[0] * sky_reflectance;
    const vec4 final_color = vec4(final_rgb, faceColor.a);
    #else
    const float alpha = AverageValue(gsIn[0].color.a, gsIn[1].color.a, gsIn[2].color.a);
    const vec4 final_color = vec4(AverageValue(vertex_colors), alpha);
    #endif // ONE_VERTEX_LIGHTING

    for (int i = 0; i < gl_in.length(); i++)
    {
        gl_Position = gl_in[i].gl_Position;
        gsOut.position = gsIn[i].position;
        gsOut.color = final_color;
        EmitVertex();
    }
    
    EndPrimitive();
}