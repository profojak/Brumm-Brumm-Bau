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
    mat4 baseModel;
    vec4 colorSpin;
} pc;

layout(location = 0) out vec3 vWorldPosition;
layout(location = 1) out vec3 vWorldNormal;

void main()
{
    float a = pc.colorSpin.w;
    mat3 spinY = mat3(
        cos(a),  0.0, sin(a),
        0.0,     1.0, 0.0,
       -sin(a),  0.0, cos(a)
    );

    vec3 spunPos    = spinY * inPosition;
    vec3 spunNormal = spinY * inNormal;

    vec4 world = pc.baseModel * vec4(spunPos, 1.0);

    vWorldPosition = world.xyz;
    vWorldNormal   = mat3(transpose(inverse(pc.baseModel))) * spunNormal;

    gl_Position = cameraData.proj * cameraData.view * world;
}
