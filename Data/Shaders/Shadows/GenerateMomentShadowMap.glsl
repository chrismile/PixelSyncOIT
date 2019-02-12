#define REQUIRE_INVOCATION_INTERLOCK
#define MOMENT_GENERATION 1

// Propagate defines to normal MomentOIT names (and undefine if already defined by MBOIT)
#ifdef NUM_MOMENTS
#undef ROV
#undef NUM_MOMENTS
#undef SINGLE_PRECISION
#undef TRIGONOMETRIC
#undef USE_R_RG_RGBA_FOR_MBOIT6
#endif
#define ROV ROV_SHADOW
#define NUM_MOMENTS NUM_MOMENTS_SHADOW
#define SINGLE_PRECISION SINGLE_PRECISION_SHADOW
#define TRIGONOMETRIC TRIGONOMETRIC_SHADOW
#define USE_R_RG_RGBA_FOR_MBOIT6 USE_R_RG_RGBA_FOR_MBOIT6_SHADOW

#define SHADOW_MAPPING_MOMENTS_GENERATION
#include "MomentShadowHeader.glsl"
#include "MomentOIT.glsl"

void gatherFragment(vec4 color)
{
	float depth = logDepthWarp(-screenSpacePosition.z, logDepthMinShadow, logDepthMaxShadow);

    float transmittance = 1.0 - color.a;
    ivec2 addr2D = ivec2(gl_FragCoord.xy);

    memoryBarrierImage();
    generateMoments(depth, transmittance, addr2D, MomentOIT.wrapping_zone_parameters);
}
