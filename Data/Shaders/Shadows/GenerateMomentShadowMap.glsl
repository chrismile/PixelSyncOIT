#define REQUIRE_INVOCATION_INTERLOCK
#define MOMENT_GENERATION 1

#define SHADOW_MAPPING_MOMENTS_GENERATION
#include "MomentShadowHeader.glsl"
//#include "MomentOIT.glsl"

void gatherFragment(vec4 color)
{
	float depth = logDepthWarp(-screenSpacePosition.z, logDepthMinShadow, logDepthMaxShadow);

    float transmittance = 1.0 - color.a;
    ivec2 addr2D = ivec2(gl_FragCoord.xy);

    memoryBarrierImage();
    //generateMoments(depth, transmittance, addr2D, MomentOIT.wrapping_zone_parameters);
}
