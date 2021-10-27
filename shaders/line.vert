#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 from;
layout(location = 2) in vec3 to;
layout(location = 3) in float width;

layout(location = 0) out vec4 colorOut;

vec2 positions[6] = vec2[](
    vec2(1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(0.0, -1.0),
    vec2(0.0, -1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

vec2 rotate(vec2 i, vec2 dir) {
    dir = normalize(dir);

    return vec2(i.x * dir.x - i.y * dir.y,
                i.x * dir.y + i.y * dir.x);
}

void main() {
    vec4 fromP = ubo.proj * ubo.view * vec4(from, 1);
    vec4 toP = ubo.proj * ubo.view * vec4(to, 1);

    vec2 from2d = fromP.xy/fromP.w;
    vec2 to2d = toP.xy/toP.w;
    vec2 dir = to2d - from2d;
    float len = length(dir);

    vec2 point = positions[gl_VertexIndex];
    point = vec2(point.x*len, point.y*width);
    point = rotate(point, dir);

    gl_Position = vec4(from2d + point, 0, 1);
    colorOut = color;
}
