#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inWorldPos;

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

layout(set = 0, binding = 4) uniform LightSpace {
    mat4 lightVP[3];
    vec4 cascadeSplits;
} lightSpace;

layout(push_constant) uniform PushConstant {
    mat4 inModel;
    vec4 debugColor;
    int renderMode;
    int objIndex;
};

// DebugRenderMode Enum
#define RENDER_MODE_OBJ_INDEX  0
#define RENDER_MODE_VIS_NORMAL 1
#define RENDER_MODE_VIS_UV     2
#define RENDER_MODE_WIREFRAME  3
#define RENDER_MODE_GAME       4

void main()
{
    vec3 color = vec3(debugColor.rgb);
    if (renderMode == RENDER_MODE_VIS_NORMAL)
    {
        color = normalize(inNormal) * 0.5 + 0.5;
    }
    else if (renderMode == RENDER_MODE_VIS_UV)
    {
        float depth = -(cameraData.view * vec4(inWorldPos, 1.0)).z;
        int cascadeIndex = 0;
        if (depth > lightSpace.cascadeSplits.x) {
            cascadeIndex = 1;
        }
        if (depth > lightSpace.cascadeSplits.y) {
            cascadeIndex = 2;
        }

        vec3 cascadeColor = vec3(1.0, 0.0, 0.0);
        if (cascadeIndex == 1) {
            cascadeColor = vec3(0.0, 1.0, 0.0);
        } else if (cascadeIndex == 2) {
            cascadeColor = vec3(0.0, 0.0, 1.0);
        }
        if (depth > lightSpace.cascadeSplits.z) {
            cascadeColor = vec3(1.0, 1.0, 1.0);
        }

        color = mix(vec3(inUV, 0.0), cascadeColor, 0.5);
    }

    outColor = vec4(color, 1.0);
}
