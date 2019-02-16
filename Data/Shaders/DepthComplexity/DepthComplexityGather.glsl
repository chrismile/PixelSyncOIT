
#include "DepthComplexityHeader.glsl"

#define REQUIRE_INVOCATION_INTERLOCK

out vec4 fragColor;

void gatherFragment(vec4 color)
{
    uint x = uint(gl_FragCoord.x);
    uint y = uint(gl_FragCoord.y);
    uint pixelIndex = addrGen(uvec2(x,y));

    memoryBarrierBuffer();
    atomicAdd(numFragmentsBuffer[addrGen(uvec2(x,y))], 1);

    // Just compute something so that normal and color uniform variables aren't optimized out.
    // Color buffer write is disabled anyway.
    fragColor = color;
}
