#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;

layout(push_constant) uniform PC { mat4 vp; } pc;

layout(set = 0, binding = 0) readonly buffer Transforms {
    mat4 transforms[];
};

layout(location = 0) out vec3 outNormal;

void main()
{
    mat4 model  = transforms[gl_InstanceIndex];
    gl_Position = pc.vp * model * vec4(pos, 1.0);
    outNormal   = normal;
}
