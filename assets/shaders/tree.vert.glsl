#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Per-instance model matrix, supplied through a second (instance-rate) vertex
// binding so all trees can be drawn in a single instanced draw call.
layout(location = 3) in mat4 inInstanceModel;

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
    mat4 inModel;            // unused for trees (instance matrix is used instead)
    vec4 inColor;
    vec4 materialProperties; // ka, kd, ks, alpha
    int  showFresnel;
    int  useExampleTexture;
} pc;

layout(location = 0) out VertexData {
    vec3 worldPosition;
    vec3 worldNormal;
    vec2 uv;
} vertOut;

void main()
{
    vec4 worldPosition = inInstanceModel * vec4(inPosition, 1.0);

    vertOut.worldPosition = worldPosition.xyz;
    vertOut.worldNormal   = mat3(transpose(inverse(inInstanceModel))) * inNormal;
    vertOut.uv            = inUV;
    gl_Position           = cameraData.proj * cameraData.view * worldPosition;
}
