#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    mat4 viewInv;
    mat4 projInv;
    mat4 projview;
    vec3 viewPos;
    vec2 resolution;
    float time;
} scene;

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 from;
layout(location = 2) in vec3 to;
layout(location = 3) in float width;

layout(location = 0) out vec4 colorOut;

vec2 positions[6] = vec2[](
    vec2(1.0, 1),
    vec2(1.0, -1),
    vec2(0.0, -1),
    vec2(0.0, -1),
    vec2(0.0, 1),
    vec2(1.0, 1)
);

vec2 rotate(vec2 i, vec2 dir) {
    dir = normalize(dir);

    return vec2(i.x * dir.x - i.y * dir.y,
                i.x * dir.y + i.y * dir.x);
}

void main() {
    vec4 fromP = scene.proj * scene.view * vec4(from, 1);
    vec4 toP = scene.proj * scene.view * vec4(to, 1);

    vec4 fromC = fromP;
    fromC.xyz /= fromP.w;
    vec4 toC = toP;
    toC.xyz /= toP.w;

    vec2 dir = toC.xy - fromC.xy;
    float len = length(dir * scene.resolution);

    vec2 point = positions[gl_VertexIndex % 6];
    point = vec2(point.x*len, point.y * width);
    point = rotate(point, dir * scene.resolution);
    point.xy /= scene.resolution;

    if (gl_VertexIndex == 0 || gl_VertexIndex == 1 || gl_VertexIndex == 5) {
        vec4 result = vec4(fromC.xy + point, toC.z, toC.w);
        result.xzy *= result.w;
        gl_Position = result;
    }
    else {
        vec4 result = vec4(fromC.xy + point, fromC.z, fromC.w);;
        result.xzy *= result.w;
        gl_Position = result;
    }

    colorOut = color;
}
