
#define REQUIRE_INVOCATION_INTERLOCK
#define MOMENT_GENERATION 1

uniform int viewportW;

#include "MBOITHeader.glsl"
#include "MomentOIT.glsl"
#include "TiledAddress.glsl"

out vec4 fragColor;

void gatherFragment(vec4 color)
{
	float depth = logDepthWarp(-screenSpacePosition.z, logDepthMin, logDepthMax); // gl_FragCoord.z
	//float depth = gl_FragCoord.z * 2.0 - 1.0;
	float transmittance = 1.0 - color.a;
	ivec2 addr2D = addrGen2D(ivec2(gl_FragCoord.xy));

	memoryBarrierImage();
	generateMoments(depth, transmittance, addr2D, MomentOIT.wrapping_zone_parameters);

	fragColor = vec4(color);
}

