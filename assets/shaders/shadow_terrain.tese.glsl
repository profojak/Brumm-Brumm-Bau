#version 460

// Shadow terrain tessellation evaluation shader.
// Based on terrain.tese.glsl but transforms by light VP matrix instead of camera VP.
// Displaces vertices using the heightmap and outputs depth from the light's perspective.

layout(quads, fractional_odd_spacing, ccw) in;

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec2 inUV[];
layout(location = 2) patch in float inLOD;

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
    mat4 lightVP;
} pc;

layout(set = 1, binding = 0) uniform TerrainTessellationData {
    float tessellationFactor;
} tessData;

layout(set = 1, binding = 1) uniform sampler2D heightMap;

const float HEIGHT_SCALE = 60.0;
const float TERRAIN_WORLD_SIZE = 310.0;

float sampleHeight(vec2 uv)
{
    return textureLod(heightMap, uv, 0.0).r;
}

void main()
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;
    vec3 worldPos = mix(mix(inPosition[0], inPosition[3], u), mix(inPosition[1], inPosition[2], u), v);
    vec2 interpolatedUV = mix(mix(inUV[0], inUV[3], u), mix(inUV[1], inUV[2], u), v);
    float height = sampleHeight(interpolatedUV);
    worldPos.y += height * HEIGHT_SCALE;
    gl_Position = pc.lightVP * vec4(worldPos, 1.0);
}
