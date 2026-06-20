#version 460

layout (location = 0) in  vec2 inUV;
layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D uDepth;

layout (push_constant) uniform PushConstants
{
    vec4  edgeColor;
    float threshold;
    int   visualizeDepth;
    float nearPlane;
    float farPlane;
} pc;

void main()
{
    const ivec2 size  = textureSize(uDepth, 0);
    const vec2  texel = 1.0 / vec2(size);

    // 3x3 depth neighborhood
    const float tl = texture(uDepth, inUV + vec2(-texel.x, -texel.y)).r;
    const float tm = texture(uDepth, inUV + vec2( 0.0,     -texel.y)).r;
    const float tr = texture(uDepth, inUV + vec2( texel.x, -texel.y)).r;
    const float ml = texture(uDepth, inUV + vec2(-texel.x,  0.0     )).r;
    const float mr = texture(uDepth, inUV + vec2( texel.x,  0.0     )).r;
    const float bl = texture(uDepth, inUV + vec2(-texel.x,  texel.y)).r;
    const float bm = texture(uDepth, inUV + vec2( 0.0,      texel.y)).r;
    const float br = texture(uDepth, inUV + vec2( texel.x,  texel.y)).r;

    // Sobel gradients
    const float gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    const float gy = -tl - 2.0 * tm - tr + bl + 2.0 * bm + br;
    const float g  = sqrt(gx * gx + gy * gy);

    // Smooth step around the threshold to avoid hard aliasing.
    float edge = smoothstep(pc.threshold, pc.threshold * 2.0 + 1e-6, g);
    edge = clamp(edge, 0.0, 1.0);

    if (pc.visualizeDepth != 0)
    {
        // Depth visualization: linearize the non-linear depth buffer value
        const float d  = texture(uDepth, inUV).r;
        float linearEye = pc.nearPlane * pc.farPlane /
                          (pc.farPlane - d * (pc.farPlane - pc.nearPlane));
        float linear    = (linearEye - pc.nearPlane) /
                          (pc.farPlane - pc.nearPlane);
        linear = clamp(linear, 0.0, 1.0);
        const vec3  depthViz = vec3(1.0 - linear);
        outFragColor = vec4(mix(depthViz, pc.edgeColor.rgb, edge), 1.0);
    }
    else
    {
        // Blend edges on top
        outFragColor = vec4(pc.edgeColor.rgb, edge);
    }
}
