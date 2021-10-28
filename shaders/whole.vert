#version 450

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 near;
layout (location = 2) out vec3 far;

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec2 resolution;
} ubo;

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}


void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 position = outUV * 2.0f + -1.f;

    near = UnprojectPoint(position.x, position.y, 0.f, ubo.view, ubo.proj);
    far = UnprojectPoint(position.x, position.y, 1.f, ubo.view, ubo.proj);
    gl_Position = vec4(position, 0.0f, 1.0f);
}
