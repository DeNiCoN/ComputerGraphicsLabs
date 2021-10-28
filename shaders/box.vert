#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec2 resolution;
} ubo;

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 center;
layout(location = 2) in vec3 dimentions;

layout(location = 0) out vec4 colorOut;

vec3 positions[16] = vec3[](
    vec3(0.5, 0.5, 0.5),
    vec3(0.5, 0.5, -0.5),
    vec3(0.5, -0.5, -0.5),
    vec3(0.5, -0.5, 0.5),
    vec3(0.5, 0.5, 0.5),
    vec3(-0.5, 0.5, 0.5),
    vec3(-0.5, 0.5, -0.5),
    vec3(0.5, 0.5, -0.5),
    vec3(0.5, -0.5, -0.5),
    vec3(-0.5, -0.5, -0.5),
    vec3(-0.5, 0.5, -0.5),
    vec3(-0.5, -0.5, -0.5),
    vec3(-0.5, -0.5, 0.5),
    vec3(0.5, -0.5, 0.5),
    vec3(-0.5, -0.5, 0.5),
    vec3(-0.5, 0.5, 0.5)
);

void main() {
    gl_Position = ubo.proj * ubo.view * vec4(center + positions[gl_VertexIndex] * dimentions, 1);
    colorOut = color;
}
