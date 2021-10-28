#version 450
#define MAX_STEPS 400
#define MAX_DIST 100.
#define SURF_DIST .0001

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 near;
layout(location = 2) in vec3 far;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec2 resolution;
} ubo;

float Torus(vec3 p, vec2 r){
    float x = length(p.xz)-r.x;
    return length (vec2(x,p.y))-r.y;

}

float surf(vec3 p) {
    float d = abs(sin(p.x) + cos(p.z) + p.y) - 0.01;

    return (d/1.71);
}

float GetDist(vec3 p) {
    vec4 s1 = vec4(-3.1, 2.9, 1.3, 1);
    float sphere1 = length(p-s1.xyz)-s1.w;

    vec4 s2 = vec4(0, 0, 0, 6);
    float sphere2 = length(p-s2.xyz)-s2.w;

    float torus = max(Torus(p-vec3(-3,2,0), vec2(1.5, .3)), -sphere1);

    float s = max(surf(p), sphere2);

    float d = min(s, torus);

    return d;
}

float RayMarch(vec3 ro, vec3 rd) {
    float dO=0.;

    for(int i=0; i<MAX_STEPS; i++) {
        vec3 p = ro + rd*dO;
        float dS = GetDist(p);
        dO += dS;
        if(dO>MAX_DIST || dS<SURF_DIST) break;
    }

    return dO;
}

vec3 GetNormal(vec3 p) {
    float d = GetDist(p);
    vec2 e = vec2(.01, 0);

    vec3 n = d - vec3(
        GetDist(p-e.xyy),
        GetDist(p-e.yxy),
        GetDist(p-e.yyx));

    return normalize(n);
}

float GetLight(vec3 p) {
    vec3 lightPos = vec3(0, 5, 6);

    vec3 l = normalize(lightPos-p);
    vec3 n = GetNormal(p);

    float dif = clamp(dot(n, l), 0.0, 1.);
    float d = RayMarch(p+n*SURF_DIST*2., l);
    if(d<length(lightPos-p)) dif *= .1;

    return max(dif, 0.1);
}

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = ubo.proj * ubo.view * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

void main()
{
    vec3 col = vec3(0);

    vec3 ro = near;
    vec3 rd = normalize(far - near);

    float d = RayMarch(ro, rd);

    vec3 p = ro + rd * d;

    float dif = GetLight(p);
    col = vec3(dif);

    //discard;
    fragColor = vec4(col, float(d < MAX_DIST));
    gl_FragDepth = computeDepth(p);
}
