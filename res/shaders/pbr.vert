
#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    mat4 viewInv;
    mat4 projInv;
    mat4 projview;
    vec2 resolution;
    float time;
} scene;

layout( push_constant ) uniform Constants {
    mat4 model;
} constants;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec4 tangent;

layout(location = 0) out vec3 normalOut;
layout(location = 1) out vec3 FragPos;
layout(location = 2) out vec2 uvOut;
layout(location = 3) out vec3 tang;
layout(location = 4) out vec3 bin;

//void main()
//{
//    vec2 outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
//    vec2 positio = outUV * 2.0f + -1.f;
//
//    gl_Position = vec4(positio, 0.0f, 1.0f);
//}
void main() {
    FragPos = vec3(constants.model * vec4(position, 1.f));
    gl_Position = scene.projview * vec4(FragPos, 1.f);
    uvOut = uv;

    mat3 normalMatrix = transpose(inverse(mat3(constants.model)));
    vec3 T = normalize(normalMatrix * vec3(tangent));
    vec3 N = normalize(normalMatrix * normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = tangent.w * cross(N, T);

    tang = T;
    bin = B;
    normalOut = N;
}
