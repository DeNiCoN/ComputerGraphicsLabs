#version 450

layout(location = 0) in vec3 barycentric;

layout(location = 0) out vec4 color;

void main() {
    if (barycentric.x < 0.01 || barycentric.y < 0.01 || barycentric.z < 0.01)
    {
        color = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else
    {
        color = vec4(0.9, 0.4, 0.05, 1.0);
    }
}
