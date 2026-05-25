#version 460
/*
 * Copyright 2023  TU Wien, Institute of Visual Computing & Human-Centered Technology.
 * This file is part of the GCG Lab Framework and must not be redistributed.
 *
 * Original version created by Lukas Gersthofer and Bernhard Steiner.
 * Vulkan edition created by Johannes Unterguggenberger (junt@cg.tuwien.ac.at).
 *
 * 2026: Adapted for the PTVC framework by Balázs Kovács.
 */

layout(set = 0, binding = 0) uniform UniformBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 position;
    float nearPlane;
    float farPlane;
} ub_data;

layout(set = 0, binding = 1) uniform DirectionalLight {
    vec4 color;
    vec4 direction;
} dl_data;

layout(set = 0, binding = 2) uniform PointLight {
    vec4 color;
    vec4 position;
    vec4 attenuation;
} pl_data;

layout(set = 0, binding = 3) uniform sampler2DShadow shadowMap;

layout(set = 0, binding = 4) uniform LightSpace {
    mat4 lightVP[3];
    vec4 cascadeSplits;
} lightSpace;

layout(set = 1, binding = 0) uniform sampler2D uTexture;

layout(location = 0) in VertexData {
    vec3 positionWorld;
    vec3 normalWorld;
    vec2 uv;
} frag_in;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstant {
    mat4 inModel;
    vec4 inColor;
    vec4 materialProperties; // ka, kd, ks, alpha
    int showFresnel;
    int useExampleTexture;
} pc;

vec3 phong(vec3 n, vec3 l, vec3 v, vec3 diffuseC, float diffuseF, vec3 specularC, float specularF, float alpha, bool attenuate, vec3 attenuation) {
    float d = length(l);
    l = normalize(l);
    float att = 1.0;
    if (attenuate) {
        att = 1.0f / (attenuation.x + d * attenuation.y + d * d * attenuation.z);
    }
    vec3 r = reflect(-l, n);
    return (diffuseF * diffuseC * max(0, dot(n, l)) + specularF * specularC * pow(max(0, dot(r, v)), alpha)) * att;
}

// Computes the reflection direction for an incident vector I about normal N,
// and clamps the reflection to a maximum of 180, i.e. the reflection vector
// will always lie within the hemisphere around normal N.
// Aside from clamping, this function produces the same result as GLSL's reflect function.
vec3 clampedReflect(vec3 I, vec3 N)
{
    return I - 2.0 * min(dot(N, I), 0.0) * N;
}

// Gets the reflected color value from a a certain position, from
// a certain direction INSIDE of a cornell box of size 3 which is
// positioned at the origin.
// positionWS:  Position inside the cornell box for which to get the
//              reflected color value for from -directionWS.
// directionWS: Outgoing direction vector (from positionWS towards the
//              outside) for which to get the reflected color value for.
vec3 getCornellBoxReflectionColor(vec3 positionWS, vec3 directionWS) {
    vec3 P0 = positionWS;
    vec3 V = normalize(directionWS);

    const float boxSize = 1.5;
    vec4[5] planes = {
            vec4(-1.0, 0.0, 0.0, -boxSize), // left
            vec4(1.0, 0.0, 0.0, -boxSize), // right
            vec4(0.0, 1.0, 0.0, -boxSize), // top
            vec4(0.0, -1.0, 0.0, -boxSize), // bottom
            vec4(0.0, 0.0, -1.0, -boxSize) // back
        };
    vec3[5] colors = {
            vec3(0.49, 0.06, 0.22), // left
            vec3(0.0, 0.13, 0.31), // right
            vec3(0.96, 0.93, 0.85), // top
            vec3(0.64, 0.64, 0.64), // bottom
            vec3(0.76, 0.74, 0.68) // back
        };

    for (int i = 0; i < 5; ++i) {
        vec3 N = planes[i].xyz;
        float d = planes[i].w;
        float denom = dot(V, N);
        if (denom <= 0) continue;
        float t = -(dot(P0, N) + d) / denom;
        vec3 P = P0 + t * V;
        float q = boxSize + 0.01;
        if (P.x > -q && P.x < q && P.y > -q && P.y < q && P.z > -q && P.z < q) {
            return colors[i];
        }
    }
    return vec3(0.0, 0.0, 0.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float computeShadowPCF(vec4 lightSpacePos, int cascadeIndex)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0
            || projCoords.y < 0.0 || projCoords.y > 1.0)
    {
        return 1.0;
    }

    projCoords.x = (projCoords.x / 3.0) + (float(cascadeIndex) / 3.0);
    float bias = 0.00015;
    if (cascadeIndex == 1) bias = 0.0003;
    if (cascadeIndex == 2) bias = 0.0005;
    float depth = projCoords.z - bias;

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(-0.5, -0.5) * texelSize, depth));
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(0.5, -0.5) * texelSize, depth));
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(-0.5, 0.5) * texelSize, depth));
    shadow += texture(shadowMap, vec3(projCoords.xy + vec2(0.5, 0.5) * texelSize, depth));

    return shadow * 0.25;
}

void main() {
    vec3 n = normalize(frag_in.normalWorld);
    vec3 v = normalize(frag_in.positionWorld - ub_data.position.xyz);
    vec3 R = normalize(clampedReflect(v, n));
    vec3 reflectionColor = getCornellBoxReflectionColor(frag_in.positionWorld, R);
    vec3 F0 = vec3(0.1); // <-- some kind of plastic
    vec3 reflectivity = fresnelSchlick(dot(n, -v), F0);

    // Texture or inColor depending on "useExampleTexture"
    vec3 diffuseColor = (pc.useExampleTexture == 1) ? texture(uTexture, frag_in.uv).rgb : pc.inColor.rgb;

    // Start with ambient illumination contribution:
    vec3 color = diffuseColor * pc.materialProperties.x;
    float diffuseF = pc.materialProperties.y;
    float specularF = pc.materialProperties.z;
    float specularA = pc.materialProperties.w;

    float depth = -(ub_data.view * vec4(frag_in.positionWorld, 1.0)).z;
    int cascadeIndex = 0;
    if (depth > lightSpace.cascadeSplits.x) {
        cascadeIndex = 1;
    }
    if (depth > lightSpace.cascadeSplits.y) {
        cascadeIndex = 2;
    }

    float shadowFactor = 1.0;
    if (depth <= lightSpace.cascadeSplits.z) {
        vec4 lightSpacePosition = lightSpace.lightVP[cascadeIndex] * vec4(frag_in.positionWorld, 1.0);
        shadowFactor = computeShadowPCF(lightSpacePosition, cascadeIndex);
    }

    // Add directional light's contribution (shadowed):
    color += phong(
            n,
            -dl_data.direction.xyz,
            -v,
            dl_data.color.rgb * diffuseColor, diffuseF,
            dl_data.color.rgb, specularF, specularA,
            false, vec3(1.0)
        ) * shadowFactor;

    // Add point light's contribution (not shadowed):
    color += phong(
            n,
            pl_data.position.xyz - frag_in.positionWorld,
            -v,
            pl_data.color.rgb * diffuseColor, diffuseF,
            pl_data.color.rgb, specularF, specularA,
            true, pl_data.attenuation.xyz
        );

    // Write color for the current fragment:
    out_color = vec4(color, 1.0);

    if (pc.showFresnel == 1)
    {
        vec3 mixedColor = mix(color, reflectionColor, reflectivity);
        out_color = vec4(mixedColor, 1.0);
    }
}
