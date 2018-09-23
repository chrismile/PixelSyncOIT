
#include "ColorPack.glsl"
#include "HTHeader.glsl"
#include "TiledAddress.glsl"

#define REQUIRE_INVOCATION_INTERLOCK

out vec4 fragColor;

// Adapted version of Hybrid Transparency [Maule et al. 2014]
void hybridTransparencyBlending(in HTFragmentNode frag, inout HTFragmentNode list[MAX_NUM_NODES],
        inout HTFragmentTail tail)
{
	HTFragmentNode temp;
	// Use bubble sort to insert new fragment node (single pass)
	for (int i = 0; i < MAX_NUM_NODES; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    if (frag.depth != DISTANCE_INFINITE) {
    	// Update tail (accumulates result)
    	vec4 fragColor = unpackColorRGBA(frag.premulColor);
        tail.accumColor.rgb += fragColor.rgb;
        tail.accumColor.a += 1.0 - fragColor.a;
        tail.accumFragCount += 1u;
    }
}


void gatherFragment(vec4 color)
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	HTFragmentNode frag;
	frag.depth = gl_FragCoord.z;
	frag.premulColor = packColorRGBA(vec4(color.rgb * color.a, 1.0 - color.a));


    // Begin of actual algorithm code
	HTFragmentNode nodeArray[MAX_NUM_NODES];
	HTFragmentTail tail;

	// Read data from SSBO
	memoryBarrierBuffer();
	loadFragmentNodes(pixelIndex, nodeArray);
	unpackFragmentTail(tails[pixelIndex], tail);

	// Insert node to list
	hybridTransparencyBlending(frag, nodeArray, tail);

	// Write back data to SSBO
	storeFragmentNodes(pixelIndex, nodeArray);
	packFragmentTail(tail, tails[pixelIndex]);

	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
