#version 460

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inUV;

//  - chars[16]  : atlas index (0..9 digits, 10 = ':', 11 = '/') per slot
//  - x, y       : bottom-left pixel position of the first character
//                 (y measured from the bottom of the screen)
//  - charW/H    : on-screen size of a single character in pixels
//  - gap        : horizontal pixel gap between consecutive characters
//  - screenW/H  : swapchain extent in pixels
//  - count      : number of character slots to render
layout(push_constant, std430) uniform PC
{
  uint  chars[16];
  float x;
  float y;
  float charW;
  float charH;
  float gap;
  float screenW;
  float screenH;
  uint  count;
} pc;

layout(location = 0) out vec2 vUV;

const float kAtlasChars = 12.0;

void main()
{
  uint slot   = uint(gl_VertexIndex) / 4u;
  uint corner = uint(gl_VertexIndex) % 4u;

  if(slot >= pc.count)
  {
    gl_Position = vec4(-2.0, -2.0, 0.0, 1.0);
    vUV         = vec2(0.0);
    return;
  }

  vec2 off = vec2(float(corner & 1u), float((corner >> 1u) & 1u));

  uint ci = pc.chars[slot];
  vUV = vec2((float(ci) + off.x) / kAtlasChars, 1.0 - off.y);
  float px = pc.x + (float(slot) * (pc.charW + pc.gap)) + off.x * pc.charW;
  float py = pc.y + off.y * pc.charH;
  vec2 ndc = vec2(px / pc.screenW * 2.0 - 1.0,
                  1.0 - py / pc.screenH * 2.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
}
