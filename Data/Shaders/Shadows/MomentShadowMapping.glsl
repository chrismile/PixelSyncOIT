uniform mat4 lightViewMatrix;
uniform mat4 lightSpaceMatrix;

uniform float logDepthMinShadow;
uniform float logDepthMaxShadow;

// Maps depth to range [-1,1] with logarithmic scale
#ifndef MOMENT_BASED_OIT
float logDepthWarp(float z, float logmin, float logmax)
{
    return (log(z) - logmin) / (logmax - logmin) * 2.0 - 1.0;
}
#endif

#include "MomentOITShadow.glsl"

float getShadowFactor(vec4 worldPosition)
{
    vec4 fragPosLightView = lightViewMatrix * worldPosition;
    vec4 fragPosLight = lightSpaceMatrix * worldPosition;
    vec3 projCoords = fragPosLight.xyz / fragPosLight.w * 0.5 + 0.5;

    vec2 shadowMapAddr2D = projCoords.xy;
    float depth = logDepthWarp(-fragPosLightView.z, logDepthMinShadow, logDepthMaxShadow);

    float transmittance_at_depth = 1.0;
    float total_transmittance = 1.0;  // exp(-b_0)
    resolveMomentsShadow(transmittance_at_depth, total_transmittance, depth, shadowMapAddr2D);

    return transmittance_at_depth;
}
