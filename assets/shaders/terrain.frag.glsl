#version 460

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in float inLOD;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 position;
    float nearPlane;
    float farPlane;
} cameraData;

layout(set = 1, binding = 0) uniform TerrainTessellationData {
    float tessellationFactor;
} tessData;

layout(set = 1, binding = 1) uniform sampler2D heightMap;

// Triplanar terrain textures
layout(set = 1, binding = 2) uniform sampler2D terrainFront; // Z-axis: front/back
layout(set = 1, binding = 3) uniform sampler2D terrainSide; // X-axis: left/right
layout(set = 1, binding = 4) uniform sampler2D terrainUp; // Y-axis: up/down

layout(push_constant) uniform TerrainPushConstant {
    int debugRenderMode;
} pc;

// DebugRenderMode Enum
#define RENDER_MODE_OBJ_INDEX  0
#define RENDER_MODE_VIS_NORMAL 1
#define RENDER_MODE_VIS_UV     2
#define RENDER_MODE_WIREFRAME  3
#define RENDER_MODE_GAME       4

const float TEX_SCALE = 0.05;
const float BLEND_SHARPNESS = 8.0;

vec3 colorLOD(float lod)
{
    float t = log2(lod) / 3.0;
    return vec3(clamp((1.0 - t), 0.0, 1.0), 0.0, clamp(t, 0.0, 1.0));
}

void main()
{
    vec3 color;
    if (pc.debugRenderMode == RENDER_MODE_OBJ_INDEX)
    {
        color = colorLOD(inLOD);
    }
    else if (pc.debugRenderMode == RENDER_MODE_VIS_NORMAL)
    {
        color = normalize(inNormal) * 0.5 + 0.5;
    }
    else if (pc.debugRenderMode == RENDER_MODE_VIS_UV)
    {
        color = vec3(inUV, 0.0);
    }
    else if (pc.debugRenderMode == RENDER_MODE_WIREFRAME)
    {
        color = colorLOD(inLOD);
    }
    else if (pc.debugRenderMode == RENDER_MODE_GAME)
    {
        vec3 N = normalize(inNormal);

        // Project world-space position onto the three planes
        vec2 uvX = inWorldPosition.zy * TEX_SCALE;
        vec2 uvY = inWorldPosition.xz * TEX_SCALE;
        vec2 uvZ = inWorldPosition.xy * TEX_SCALE;

        vec3 colX = texture(terrainSide, uvX).rgb;
        vec3 colY = texture(terrainUp, uvY).rgb;
        vec3 colZ = texture(terrainFront, uvZ).rgb;

        // Blend weights from normal
        vec3 blend = abs(N);
        blend = pow(blend, vec3(BLEND_SHARPNESS));
        blend /= (blend.x + blend.y + blend.z);

        // Blend the three axis-aligned samples
        color = colX * blend.x + colY * blend.y + colZ * blend.z;
    }
    outColor = vec4(color, 1.0);
}
