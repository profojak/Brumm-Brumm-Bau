#version 460

layout(vertices = 4) out;

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec2 inUV[];

layout(location = 0) out vec3 outPosition[];
layout(location = 1) out vec2 outUV[];
layout(location = 2) patch out float outLOD;

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

// Base distance thresholds
const float LOD_DISTANCES[7] = {
        1.0,
        3.0,
        6.0,
        10.0,
        16.0,
        23.0,
        32.0
    };

float getTessellationLevel(vec3 p0, vec3 p1, float lodScale)
{
    float dist = length((p0.xz + p1.xz) * 0.5 - cameraData.position.xz) / lodScale;
    for (int i = 0; i < 6; i++)
    {
        if (dist < LOD_DISTANCES[i])
            return float(1 << (6 - i));
    }
    return 1.0;
}

void main()
{
    // Pass through control points
    outPosition[gl_InvocationID] = inPosition[gl_InvocationID];
    outUV[gl_InvocationID] = inUV[gl_InvocationID];

    if (gl_InvocationID == 0)
    {
        if (tessData.tessellationFactor > 0.0)
        {
            float lodScale = max(tessData.tessellationFactor * 5.0, 0.01);

            gl_TessLevelOuter[0] = getTessellationLevel(inPosition[0], inPosition[1], lodScale);
            gl_TessLevelOuter[1] = getTessellationLevel(inPosition[3], inPosition[0], lodScale);
            gl_TessLevelOuter[2] = getTessellationLevel(inPosition[2], inPosition[3], lodScale);
            gl_TessLevelOuter[3] = getTessellationLevel(inPosition[1], inPosition[2], lodScale);
        }
        else
        {
            gl_TessLevelOuter[0] = 1.0;
            gl_TessLevelOuter[1] = 1.0;
            gl_TessLevelOuter[2] = 1.0;
            gl_TessLevelOuter[3] = 1.0;
        }

        // Inner tessellation: use the finest outer level
        float maxOuter = max(max(gl_TessLevelOuter[0], gl_TessLevelOuter[1]),
                max(gl_TessLevelOuter[2], gl_TessLevelOuter[3]));
        gl_TessLevelInner[0] = maxOuter;
        gl_TessLevelInner[1] = maxOuter;
        outLOD = maxOuter;
    }
}
