#version 450

layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;

layout(set = 1, binding = 2) uniform sampler2D specular;

void main() {
    float s = texture(specular, uv).x;
    colorOut = vec4(s, s, s, 1);
}
