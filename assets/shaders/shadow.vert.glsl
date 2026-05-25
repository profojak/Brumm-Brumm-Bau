#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform PushConstant {
    mat4 inModel;
    mat4 lightVP;
} pc;

void main()
{
    gl_Position = pc.lightVP * pc.inModel * vec4(inPosition, 1.0);
}
