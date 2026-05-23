#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outWorldPosition;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec2 outUV;

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
    vec4 inColor;
    vec4 materialProperties;
    int  showFresnel;
    int  useExampleTexture;
} pc;

void main()
{
    outWorldPosition = vec4(inPosition, 1.0);
    outColor         = pc.inColor;
    outUV            = inUV;
    gl_Position      = cameraData.proj * cameraData.view * pc.inModel * outWorldPosition;
}
