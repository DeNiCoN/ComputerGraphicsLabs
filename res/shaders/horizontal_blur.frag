#version 450

layout(location = 0) in vec2 vTexCoords;

layout(set = 0, binding = 0) uniform sampler2D uImage;

float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
// uniform float weight[7] = float[] (0.160011, 0.147866, 0.116686, 0.078632, 0.045248, 0.022234, 0.009329);
// uniform float weight[9] = float[] (0.158626, 0.146586, 0.115676, 0.077951, 0.044857, 0.022042, 0.009248, 0.003314, 0.001);
// uniform float weight[11] = float[] (0.197413, 0.174666, 0.120978, 0.065591, 0.027835, 0.009245, 0.005403, 0.001, 0.0005, 0.0005, 0.0002);

layout(location = 0) out vec3 fColor;

void main()
{
    vec3 result = vec3(0.0);

    vec2 tex_offset = 5.0 / textureSize(uImage, 0); // gets size of single texel
    result += textureLod(uImage, vTexCoords, 0).rgb * weight[0];

    for (int i = 1; i < 5; ++i)
    {
        result += textureLod(uImage, vTexCoords + vec2(tex_offset.x * i, 0.0), 0).rgb * weight[i];
        result += textureLod(uImage, vTexCoords - vec2(tex_offset.x * i, 0.0), 0).rgb * weight[i];
    }

    fColor = vec3(result);
}
