#version 460

layout(location = 0) in vec2 vUV;

layout(set = 0, binding = 0) uniform sampler2D textSampler;

layout(location = 0) out vec4 outColor;

void main()
{
  vec4 tex = texture(textSampler, vUV);
  outColor = vec4(tex.rgb, tex.a);
}
