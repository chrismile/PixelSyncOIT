uniform mat4 lightSpaceViewMatrix;
uniform mat4 lightSpaceMatrix;

uniform float logDepthMinShadow;
uniform float logDepthMaxShadow;

// For moment-based shadow mapping
#ifdef MOMENT_SHADOW_PASS
#define zeroth_moment zeroth_moment_shadow
#define moments moments_shadow
#define extra_moments extra_moments_shadow
#define zeroth_moment zeroth_moment_shadow
#define MomentOITUniformData MomentOITUniformData_Shadow
#define MomentOIT MomentOIT_Shadow
#define resolveMoments resolveMomentsShadow
#undef MOMENT_OIT_GLSL // Re-include
#undef MOMENT_GENERATION
#define ROV
#include "MomentOIT.glsl"
#endif

float getShadowFactor(vec4 worldPosition)
{
    const float bias = 0.005;
    vec4 fragPosLightView = lightSpaceViewMatrix * worldPosition;
    vec4 fragPosLight = lightSpaceMatrix * worldPosition;
    vec3 projCoords = fragPosLight.xyz / fragPosLight.w * 0.5 + 0.5;

    vec2 shadowMapAddr2D = projCoords.xy * SHADOW_MAP_SIZE;
	float depth = logDepthWarp(-fragPosLightView.z, logDepthMinShadow, logDepthMaxShadow);

    float transmittance_at_depth = 1.0;
    float total_transmittance = 1.0;  // exp(-b_0)
    resolveMomentsShadow(transmittance_at_depth, total_transmittance, depth, addr2D);

    return transmittance_at_depth;
}
