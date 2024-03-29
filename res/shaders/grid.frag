#version 450

layout(location = 0) in vec2 UV;
layout(location = 1) in vec3 near;
layout(location = 2) in vec3 far;

layout(location = 0) out vec4 outColor;

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

layout( push_constant ) uniform Constants {
    vec3 xcolor;
    vec3 zcolor;
    vec3 grid_color;
} constants;

vec4 grid(vec3 fragPos3D, float scale) {
    vec2 coord = fragPos3D.xz * scale; // use the scale variable to set the distance between the lines
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(constants.grid_color, 1.0 - min(line, 1.0));
    // z axis
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        color.xyz = constants.zcolor;
    // x axis
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        color.xyz = constants.xcolor;
    return color;
}

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = scene.projview * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

void main() {
    float t = -near.y / (far.y - near.y);
    vec3 fragPos3D = near + t * (far - near);
    float fade = min(2 / length(fragPos3D - near), 1);
    outColor = grid(fragPos3D, 10) * float(t > 0) * float(fade);
    gl_FragDepth = computeDepth(fragPos3D);
    //outColor = vec4(scene.time, 0, 0, 1);
}
