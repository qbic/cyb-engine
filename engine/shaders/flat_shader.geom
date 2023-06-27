#version 450

// Computing hard face normals on the fly in the geometry shader seems 
// to be faster than suppling hard vertex normals, AND saves vram!
#define COMPUTE_HARD_NORMALS

// Use triangle centroid as vertex position, enabling this can save 
// about 77% gpu-time on light compution.
// Disable to use the average lighting of all three vertices.
#define ONE_VERTEX_LIGHTING

#include "flat_shader.geom.glsl"
