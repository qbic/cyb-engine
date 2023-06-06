#version 450

// Computing hard face normals on the fly in the geometry shader seems 
// to be faster than suppling hard vertex normals, AND saves vram!
#define COMPUTE_HARD_NORMALS

// Only calculate lighting for one of the three vertices.
// Disable to use the average lighting of all three vertices.
#define ONE_VERTEX_LIGHTING

#include "flat_shader.geom.glsl"
