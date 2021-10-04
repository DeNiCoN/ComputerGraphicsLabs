#version 450

layout(location = 0) in vec3 barycentric;

layout(location = 0) out vec4 color;

const float lineWidth = 1.4;
const vec3 fillColor = vec3(0.0, 0.4, 0.05);

float edgeFactor() {
  vec3 d = fwidth(barycentric);
  vec3 f = step(d * lineWidth, barycentric);
  return min(min(f.x, f.y), f.z);
}

void main() {
  color = vec4(min(vec3(edgeFactor()), fillColor), 1.0);
}
