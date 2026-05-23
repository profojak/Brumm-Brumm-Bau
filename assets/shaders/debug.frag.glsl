#version 460

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstant {
    mat4 inModel;
    vec4 debugColor;
    int  renderMode;
    int  objIndex;
};

// RenderMode Enum
#define RENDER_MODE_OBJ_INDEX  0
#define RENDER_MODE_VIS_NORMAL 1
#define RENDER_MODE_VIS_UV     2

void main()
{
    vec3 color = vec3(debugColor.rgb);
    if (renderMode == RENDER_MODE_VIS_NORMAL)
    {
        color = normalize(inNormal) * 0.5 + 0.5;
    }
    else if (renderMode == RENDER_MODE_VIS_UV)
    {
        color = vec3(inUV, 0.0);
    }

    outColor = vec4(color, 1.0);
}
