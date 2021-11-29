#version 450

layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;

layout(set = 1, binding = 0) uniform sampler2D albedo;

void main() {
    colorOut = texture(albedo, uv);
}
