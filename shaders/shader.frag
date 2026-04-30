#version 450

layout(location = 0) in  vec3 inNormal;
layout(location = 0) out vec4 outColor;

void main()
{
    // vec3  lightDir = normalize(vec3(1.0, 2.0, 3.0));
    // float diff     = max(dot(normalize(inNormal), lightDir), 0.0);
    // vec3  color    = (0.15 + 0.85 * diff) * vec3(0.85, 0.85, 0.85);
    outColor = vec4(0.85, 0.85, 0.85, 1.0);
}
