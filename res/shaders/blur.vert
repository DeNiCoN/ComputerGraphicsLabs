#version 450

layout (location = 0) out vec2 outUV;


void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 position = outUV * 2.0f + -1.f;

    gl_Position = vec4(position, 0.0f, 1.0f);
}
