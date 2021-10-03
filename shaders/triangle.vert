#version 450

layout(binding = 0) uniform UniformBufferObject
{
    vec2 offset;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 barycentric;

layout(location = 0) out vec3 bary;

void main() {
    gl_Position = vec4(inPosition + ubo.offset, 0.0, 1.0);
    bary = barycentric;
}
