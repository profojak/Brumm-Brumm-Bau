#version 460

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 position;
    float nearPlane;
    float farPlane;
} cameraData;

layout(set = 0, binding = 1) uniform DirectionalLight {
    vec4 color;
    vec4 direction;
} dl_data;

layout(location = 0) in VertexData {
    vec3 worldPosition;
    vec3 worldNormal;
    vec2 uv;
} frag_in;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstant {
    mat4 inModel;
    vec4 inColor;
    vec4 materialProperties; // ka, kd, ks, alpha
    int  showFresnel;
    int  useExampleTexture;
} pc;

void main()
{
    vec3 n = normalize(frag_in.worldNormal);
    vec3 l = normalize(-dl_data.direction.xyz);

    float diffuse = max(dot(n, l), 0.0);
    float ka = pc.materialProperties.x;
    float kd = pc.materialProperties.y;
    vec3 base  = pc.inColor.rgb;
    vec3 color = base * ka + base * kd * diffuse * dl_data.color.rgb;

    out_color = vec4(color, 1.0);
}
