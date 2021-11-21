#version 450

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;

layout(set = 1, binding = 0) uniform sampler2D diffuse;

void main() {
    colorOut = vec4(texture(diffuse, uv).xyz, 1.f);
}
