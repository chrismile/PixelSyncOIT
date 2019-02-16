
#define MOMENT_SHADOW_MAP
#define MOMENT_GENERATION 0

uniform int viewportW;

#include "MBOITHeader.glsl"
#include "MomentOIT.glsl"
#include "TiledAddress.glsl"

float getShadowFactor()
{
    float depth = logDepthWarp(-screenSpacePosition.z, logDepthMin, logDepthMax);
    //float depth = gl_FragCoord.z * 2.0 - 1.0;
    //ivec2 addr2D = addrGen2D(ivec2(gl_FragCoord.xy));
    ivec2 addr2D = ivec2(gl_FragCoord.xy);
    float transmittance_at_depth = 1.0;
    float total_transmittance = 1.0;  // exp(-b_0)
    resolveMoments(transmittance_at_depth, total_transmittance, depth, addr2D);

    return transmittance_at_depth;
}

