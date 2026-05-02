#version 450
#extension GL_EXT_mesh_shader : require

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inMeshletColor;
layout(location = 2) flat in uint inLodLevel;
layout(location = 3) perprimitiveEXT flat in uint inPrimitiveID;

layout(push_constant) uniform PC {
    mat4  vp;
    vec3  meshCenter;
    float meshRadius;
    vec3  cameraPos;
    uint  meshletCount;
    uint  renderMode;
    float cotHalfFovH;
    float lodThreshold;
    uint  forceLod;
} pc;

layout(location = 0) out vec4 outColor;

vec3 lodLevelColor(uint level) {
    if (level == 0u) return vec3(0.00, 0.90, 0.80);
    if (level == 1u) return vec3(0.00, 0.55, 1.00);
    if (level == 2u) return vec3(0.15, 0.25, 1.00);
    if (level == 3u) return vec3(0.45, 0.05, 1.00);
    if (level == 4u) return vec3(0.70, 0.00, 0.90);
    if (level == 5u) return vec3(0.90, 0.00, 0.60);
    return vec3(1.00, 0.00, 0.20);
}

void main()
{
    vec3  N    = normalize(inNormal);
    float diff = max(dot(N, normalize(vec3(0.5, 1.0, 0.3))), 0.0);
    float lit  = 0.2 + 0.8 * diff;

    if (pc.renderMode == 0u) {
        outColor = vec4(vec3(0.82) * lit, 1.0);
    }
    else if (pc.renderMode == 1u) {
        outColor = vec4(inMeshletColor * lit, 1.0);
    }
    else if (pc.renderMode == 2u) {
        outColor = vec4(lodLevelColor(inLodLevel) * lit, 1.0);
    }
    else {
        // Per-triangle color — saturated HSV hash, two-sided light so all faces pop
        uint  id      = inPrimitiveID * 2246822519u;
        float hue     = fract(float(id) / 4294967296.0);
        vec3  k       = fract(vec3(1.0, 2.0/3.0, 1.0/3.0) + hue) * 6.0 - 3.0;
        vec3  rgb     = clamp(abs(k) - 1.0, 0.0, 1.0);
        float triLit  = 0.35 + 0.65 * abs(dot(N, normalize(vec3(0.5, 1.0, 0.3))));
        outColor      = vec4(rgb * triLit, 1.0);
    }
}
