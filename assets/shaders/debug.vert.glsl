#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outWorldPos;

layout (set = 0, binding = 0) uniform CameraData {
    mat4  view;
    mat4  proj;
    mat4  viewInverse;
    mat4  projInverse;
    vec4  position;
    float nearPlane;
    float farPlane;
} cameraData;

layout (push_constant) uniform PushConstant {
    mat4 inModel;
    vec4 debugColor;
    int  renderMode;
    int  objIndex;
};

void main()
{
    outNormal   = inNormal;
    outUV       = inUV;
    vec4 worldPos = inModel * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    gl_Position = cameraData.proj * cameraData.view * worldPos;
}
