
#include "KBufferHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAddress.glsl"

#define REQUIRE_INVOCATION_INTERLOCK

out vec4 fragColor;

void gatherFragment(vec4 color)
{
    if (color.a < 0.001) {
        discard;
    }

	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	// Fragment index (in nodes buffer):
	uint index = MAX_NUM_NODES*pixelIndex;

	FragmentNode frag;
	frag.color = packUnorm4x8(color);
	frag.depth = gl_FragCoord.z;

	memoryBarrierBuffer();

	// Use 1-pass bubble sort to insert new fragment
	uint numFragments = numFragmentsBuffer[pixelIndex];
	for (uint i = 0; i < numFragments; i++)
	{
		if (frag.depth < nodes[index].depth)
		{
			FragmentNode temp = frag;
			frag = nodes[index];
			nodes[index] = temp;
		}
		index++;
	}
	
	// Store the fragment at the end of the list if capacity is left
	if (numFragments < MAX_NUM_NODES) {
		numFragmentsBuffer[pixelIndex]++;
		nodes[index] = frag;
	} else {
    	// Blend with last fragment
		/*vec4 colorDst = unpackUnorm4x8(nodes[index-1].color);
		vec4 colorSrc = unpackUnorm4x8(frag.color);

		vec4 colorOut;
		colorOut.rgb = colorDst.a * colorDst.rgb + (1.0 - colorDst.a) * colorSrc.a * colorSrc.rgb;
        colorOut.a = colorDst.a + (1.0 - colorDst.a) * colorSrc.a;

    	nodes[index-1].color = packUnorm4x8(vec4(colorOut.rgb / colorOut.a, colorOut.a));*/
	}

	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
