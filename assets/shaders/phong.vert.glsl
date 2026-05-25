#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out VertexData {
    vec3 worldPosition;
    vec3 worldNormal;
    vec2 uv;
} vertOut;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 position;
    float nearPlane;
    float farPlane;
} cameraData;

layout(push_constant) uniform PushConstant {
    mat4 inModel;
    vec4 inColor;
    vec4 materialProperties;
    int showFresnel;
    int useExampleTexture;
} pc;

void main()
{
    vec4 worldPosition = pc.inModel * vec4(inPosition, 1.0);

    vertOut.worldPosition = worldPosition.xyz;
    vertOut.worldNormal = mat3(transpose(inverse(pc.inModel))) * inNormal;
    vertOut.uv = inUV;
    gl_Position = cameraData.proj * cameraData.view * worldPosition;
}
