#version 450

layout(location = 0) in vec2 vTexCoords;

layout(set = 0, binding = 0) uniform sampler2D uScene;
layout(set = 0, binding = 1) uniform sampler2D uBloomBlur;

const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 LINEARtoSRGB(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    return vec4(pow(srgbIn.xyz, vec3(GAMMA)), srgbIn.w);
}

layout(location = 0) out vec3 fColor;

void main()
{
    vec3 hdrColor = SRGBtoLINEAR(texture(uScene, vTexCoords)).rgb;
    vec3 bloomColor = SRGBtoLINEAR(texture(uBloomBlur, vTexCoords)).rgb;
    //Intensity
    bloomColor *= 0.5;
    hdrColor += bloomColor; // Additive blending

    vec3 result = hdrColor; // Tone mapping
    fColor = LINEARtoSRGB(result); // Gamma Correction
}
