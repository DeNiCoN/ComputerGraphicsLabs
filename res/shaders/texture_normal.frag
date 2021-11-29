#version 450

layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;

layout(set = 1, binding = 1) uniform sampler2D normal;

void main() {
    colorOut = vec4(texture(normal, uv).xyz, 1);
}
