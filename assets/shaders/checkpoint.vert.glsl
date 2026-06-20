#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

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
    mat4 model;
    vec4 color;
} pc;

void main() {
    gl_Position = cameraData.proj * cameraData.view * pc.model * vec4(inPosition, 1.0);
}
