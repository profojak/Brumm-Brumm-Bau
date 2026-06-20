#version 460

layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vWorldNormal;

layout(push_constant) uniform PushConstant {
    mat4 baseModel;
    vec4 colorSpin;
} pc;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 n = normalize(vWorldNormal);
    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.3));

    float diff = max(dot(n, lightDir), 0.0);
    vec3  base = pc.colorSpin.rgb;
    vec3  col  = base * (0.65 + 0.45 * diff);

    outColor = vec4(col, 1.0);
}
