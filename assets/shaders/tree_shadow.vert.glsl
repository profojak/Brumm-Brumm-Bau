#version 460

layout(location = 0) in vec3 inPosition;

// Per-instance model matrix, supplied through a second (instance-rate) vertex
// binding so all trees cast shadows in a single instanced draw call.
layout(location = 3) in mat4 inInstanceModel;

layout(push_constant) uniform PushConstant {
    mat4 lightVP;
} pc;

void main()
{
    gl_Position = pc.lightVP * inInstanceModel * vec4(inPosition, 1.0);
}
