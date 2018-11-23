// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_fragment_shader_interlock.txt
#extension GL_ARB_fragment_shader_interlock : require

#ifdef PIXEL_SYNC_ORDERED
// Use early z-test to cull transparent fragments occluded by opaque fragments.
// Additionaly, use fragment interlock.
layout(early_fragment_tests, pixel_interlock_ordered) in;
#else
// Use early z-test to cull transparent fragments occluded by opaque fragments.
// Additionaly, use fragment interlock.
layout(early_fragment_tests, pixel_interlock_unordered) in;
#endif

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer) in vec4 gl_FragCoord;

uniform int viewportW;

uniform float logDepthMin;
uniform float logDepthMax;

// Maps depth to range [0,1] with logarithmic scale
float logDepthWarp(float z)
{
	return (log(z) - logDepthMin) / (logDepthMax - logDepthMin);
	//return (z - exp(logmin)) / (exp(logmax) - exp(logmin));
}

layout (std430, binding = 1) coherent buffer MinDepthBuffer
{
	float minDepth[];
};

#include "TiledAddress.glsl"

#define REQUIRE_INVOCATION_INTERLOCK
#define OPACITY_THRESHOLD 0.1

void gatherFragment(vec4 color)
{
    ivec2 fragPos2D = ivec2(int(gl_FragCoord.x), int(gl_FragCoord.y));
	uint pixelIndex = addrGen(uvec2(fragPos2D));

	float depth = logDepthWarp(-screenSpacePosition.z);
	if (color.a > OPACITY_THRESHOLD && depth < minDepth[pixelIndex]) {
        minDepth[pixelIndex] = depth;
	}
}