#version 450

layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;

layout(set = 1, binding = 2) uniform sampler2D specular;

void main() {
    vec3 s = texture(specular, uv).xyz;
    colorOut = vec4(s, 1);
}
