#version 460

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in float inLOD;

layout(location = 0) out vec4 outColor;

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
} dl;

layout(set = 0, binding = 3) uniform sampler2DShadow shadowMap;

layout(set = 0, binding = 4) uniform LightSpace {
    mat4 lightVP[3];
    vec4 cascadeSplits;
} lightSpace;

layout(set = 1, binding = 0) uniform TerrainTessellationData {
    float tessellationFactor;
} tessData;

layout(set = 1, binding = 1) uniform sampler2D heightMap;

// Triplanar terrain textures
layout(set = 1, binding = 2) uniform sampler2D terrainFront; // Z-axis: front/back
layout(set = 1, binding = 3) uniform sampler2D terrainSide; // X-axis: left/right
layout(set = 1, binding = 4) uniform sampler2D terrainUp; // Y-axis: up/down

layout(push_constant) uniform TerrainPushConstant {
    int debugRenderMode;
} pc;

// DebugRenderMode Enum
#define RENDER_MODE_OBJ_INDEX  0
#define RENDER_MODE_VIS_NORMAL 1
#define RENDER_MODE_VIS_UV     2
#define RENDER_MODE_WIREFRAME  3
#define RENDER_MODE_GAME       4

const float TEX_SCALE = 0.05;
const float BLEND_SHARPNESS = 8.0;

const int PCF_SAMPLES = 4;

vec3 colorLOD(float lod)
{
    float t = log2(lod) / 3.0;
    return vec3(clamp((1.0 - t), 0.0, 1.0), 0.0, clamp(t, 0.0, 1.0));
}

float computeShadowPCF(vec4 lightSpacePos, int cascadeIndex, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0
            || projCoords.y < 0.0 || projCoords.y > 1.0)
    {
        return 1.0; // Outside shadow map
    }

    // Map to cascade slice in the horizontal atlas
    projCoords.x = (projCoords.x / 3.0) + (float(cascadeIndex) / 3.0);

    // Slope-scaled bias: small bias when facing the light, larger at grazing angles
    float baseBias = 0.00008;
    float maxBias  = 0.0008;
    if (cascadeIndex == 1) { baseBias = 0.00012; maxBias = 0.0012; }
    if (cascadeIndex == 2) { baseBias = 0.00018; maxBias = 0.0018; }

    float dotNL = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    float bias = max(baseBias * tan(acos(dotNL)), baseBias);
    bias = min(bias, maxBias);

    float depth = projCoords.z - bias;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;

    // 4-sample Poisson-like pattern for softer shadows
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(-0.5, -0.5) * texelSize, depth));
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(0.5, -0.5) * texelSize, depth));
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(-0.5, 0.5) * texelSize, depth));
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(0.5, 0.5) * texelSize, depth));
    shadow *= 0.25;

    return shadow;
}

void main()
{
    vec3 color;
    if (pc.debugRenderMode == RENDER_MODE_OBJ_INDEX)
    {
        color = colorLOD(inLOD);
    }
    else if (pc.debugRenderMode == RENDER_MODE_VIS_NORMAL)
    {
        color = normalize(inNormal) * 0.5 + 0.5;
    }
    else if (pc.debugRenderMode == RENDER_MODE_VIS_UV)
    {
        // Compute view-space depth
        float depth = -(cameraData.view * vec4(inWorldPosition, 1.0)).z;

        int cascadeIndex = 0;
        if (depth > lightSpace.cascadeSplits.x) {
            cascadeIndex = 1;
        }
        if (depth > lightSpace.cascadeSplits.y) {
            cascadeIndex = 2;
        }

        // Color based on cascade (blended with UV coordinates)
        vec3 cascadeColor = vec3(1.0, 0.0, 0.0);
        if (cascadeIndex == 1) {
            cascadeColor = vec3(0.0, 1.0, 0.0);
        } else if (cascadeIndex == 2) {
            cascadeColor = vec3(0.0, 0.0, 1.0);
        }
        if (depth > lightSpace.cascadeSplits.z) {
            cascadeColor = vec3(1.0, 1.0, 1.0);
        }

        color = mix(vec3(inUV, 0.0), cascadeColor, 0.5);
    }
    else if (pc.debugRenderMode == RENDER_MODE_WIREFRAME)
    {
        color = colorLOD(inLOD);
    }
    else if (pc.debugRenderMode == RENDER_MODE_GAME)
    {
        vec3 N = normalize(inNormal);

        // Project world-space position onto the three planes
        vec2 uvX = inWorldPosition.zy * TEX_SCALE;
        vec2 uvY = inWorldPosition.xz * TEX_SCALE;
        vec2 uvZ = inWorldPosition.xy * TEX_SCALE;

        vec3 colX = texture(terrainSide, uvX).rgb;
        vec3 colY = texture(terrainUp, uvY).rgb;
        vec3 colZ = texture(terrainFront, uvZ).rgb;

        // Blend weights from normal
        vec3 blend = abs(N);
        blend = pow(blend, vec3(BLEND_SHARPNESS));
        blend /= (blend.x + blend.y + blend.z);

        // Blend the three axis-aligned samples
        color = colX * blend.x + colY * blend.y + colZ * blend.z;

        vec3 ambient = color * 1.2;
        vec3 lightDir = -dl.direction.xyz;
        float NdotL = max(dot(N, normalize(lightDir)), 0.0);
        vec3 diffuse = dl.color.rgb * color * NdotL * 2.0;

        // Compute view-space depth
        float depth = -(cameraData.view * vec4(inWorldPosition, 1.0)).z;
        int cascadeIndex = 0;
        if (depth > lightSpace.cascadeSplits.x) {
            cascadeIndex = 1;
        }
        if (depth > lightSpace.cascadeSplits.y) {
            cascadeIndex = 2;
        }

        // Shadow factor from PCF
        float shadowFactor = 1.0;
        if (depth <= lightSpace.cascadeSplits.z) {
            vec4 lightSpacePosition = lightSpace.lightVP[cascadeIndex] * vec4(inWorldPosition, 1.0);
            shadowFactor = computeShadowPCF(lightSpacePosition, cascadeIndex, N, lightDir);
        }

        // Combine lighting with shadow
        color = ambient + diffuse * shadowFactor;
    }
    outColor = vec4(color, 1.0);
}
