#version 460

layout (location = 0) in vec4 inWorldPosition;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform CameraData {
    mat4  view;
    mat4  proj;
    mat4  viewInverse;
    mat4  projInverse;
    vec4  position;
    float nearPlane;
    float farPlane;
} cameraData;

layout (set = 1, binding = 0) uniform sampler2D uTexture;

layout (push_constant) uniform PushConstant {
    mat4 inModel;
    vec4 inColor;
    vec4 materialProperties;
    int  showFresnel;
    int  useExampleTexture;
} pc;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor.rgb, 1.0);
    if (pc.useExampleTexture == 1)
    {
        outColor = vec4(texture(uTexture, inUV).rgb, 1.0);
    }
}
