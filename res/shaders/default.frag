#version 450

layout(location = 0) in vec3 Normal;
layout(location = 1) in vec3 FragPos;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 colorOut;

layout(set = 1, binding = 0) uniform sampler2D diffuse;

layout(set = 0, binding = 0) uniform UniformBufferObject
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

void main() {
    vec4 objectColor = texture(diffuse, uv);
    vec3 lightColor = vec3(1.f);
    vec3 lightPos = vec3(0, 0, 0);

    if (objectColor.w < 0.001f)
        discard;

    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
    float specularStrength = 0.9;
    vec3 viewDir = normalize(scene.viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * vec3(objectColor);

    colorOut = vec4(result, 1.0);
}
