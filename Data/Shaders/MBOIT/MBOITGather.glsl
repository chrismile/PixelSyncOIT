
#define REQUIRE_INVOCATION_INTERLOCK
#define MOMENT_GENERATION 1

#include "MBOITHeader.glsl"
#include "MomentOIT.glsl"

out vec4 fragColor;

void gatherFragment(vec4 color)
{
	float depth = gl_FragCoord.z; // TODO: Linear [-1,1] ?
	float transmittance = 1.0 - color.a;

	memoryBarrierImage();
	generateMoments(depth, transmittance, gl_FragCoord.xy, MomentOIT.wrapping_zone_parameters);

	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}

