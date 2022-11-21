#version 450

layout(binding = 1) uniform sampler2D sampler0;

layout(location = 0) in FS_IN_DATA
{
    vec4 position;
    vec2 uv;
    vec4 color;
} fs_in;

layout(location = 0) out vec4 final_color;

void main() 
{
    final_color = fs_in.color * texture(sampler0, fs_in.uv);
}