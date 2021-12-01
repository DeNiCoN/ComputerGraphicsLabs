#version 450

layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;
layout(location = 1) out vec4 bloomOut;

layout(set = 1, binding = 4) uniform sampler2D ao;

void main() {
    float s = texture(ao, uv).x;
    colorOut = vec4(s, s, s, 1);
    bloomOut = vec4(0);
}
