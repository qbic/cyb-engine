#version 450
#define NO_LIGHTING

// BUG: enabling ONE_VERTEX_LIGHTING creates a bug where only the 
//      sky reflectance color is rendered
//#define ONE_VERTEX_LIGHTING
#include "flat_shader.geom.glsl"
