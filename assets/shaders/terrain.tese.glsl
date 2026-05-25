#version 460

layout(quads, fractional_odd_spacing, ccw) in;

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec2 inUV[];
layout(location = 2) patch in float inLOD;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out float outLOD;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 position;
    float nearPlane;
    float farPlane;
} cameraData;

layout(set = 1, binding = 0) uniform TerrainTessellationData {
    float tessellationFactor;
} tessData;

layout(set = 1, binding = 1) uniform sampler2D heightMap;

// Terrain parameters
const float HEIGHT_SCALE = 60.0;
const float TERRAIN_WORLD_SIZE = 310.0;

// Sample height from heightmap at given UV
float sampleHeight(vec2 uv)
{
    return textureLod(heightMap, uv, 0.0).r;
}

void main()
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // Bilinear interpolation for position and UV
    vec3 worldPos = mix(mix(inPosition[0], inPosition[3], u), mix(inPosition[1], inPosition[2], u), v);
    vec2 interpolatedUV = mix(mix(inUV[0], inUV[3], u), mix(inUV[1], inUV[2], u), v);

    // Sample height and displace along Y axis
    float height = sampleHeight(interpolatedUV);
    worldPos.y += height * HEIGHT_SCALE;

    // Compute normal from heightmap gradient using central differences
    float texelSize = 1.0 / float(textureSize(heightMap, 0).x);
    float hL = sampleHeight(interpolatedUV - vec2(texelSize, 0.0));
    float hR = sampleHeight(interpolatedUV + vec2(texelSize, 0.0));
    float hD = sampleHeight(interpolatedUV - vec2(0.0, texelSize));
    float hU = sampleHeight(interpolatedUV + vec2(0.0, texelSize));

    // Construct normal vector using gradient
    float normalY = 2.0 * TERRAIN_WORLD_SIZE * texelSize / HEIGHT_SCALE;
    vec3 normal = normalize(vec3(hL - hR, normalY, hD - hU));

    gl_Position = cameraData.proj * cameraData.view * vec4(worldPos, 1.0);
    outWorldPosition = worldPos;
    outNormal = normal;
    outUV = interpolatedUV;
    outLOD = inLOD;
}
