#version 450

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;
layout(location = 1) out vec4 bloomOut;

void main() {
    colorOut = vec4(1.f);
    bloomOut = vec4(1.f);
}
