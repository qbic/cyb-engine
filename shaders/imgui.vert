#version 450

layout(location=0) in vec2 in_position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec4 in_color;

layout(binding = 0) uniform imgui_block
{
	mat4 projection_matrix;
};

layout(location = 0) out VS_OUT_DATA
{
    vec4 position;
    vec2 uv;
    vec4 color;
} vs_out;

void main() 
{
	vs_out.position = projection_matrix * vec4(in_position.xy, 0.0, 1.0);
	vs_out.uv = vec2(in_uv.x, in_uv.y);
	vs_out.color = in_color; 
	gl_Position = vs_out.position;
}
