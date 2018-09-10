
#include "ATHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAdress.glsl"

#define REQUIRE_INVOCATION_INTERLOCK

out vec4 fragColor;

// TODO
// Adapted version of "Multi-Layer Alpha Blending" [Salvi and Vaidyanathan 2014]
void adaptiveTransparencyBlending(in ATFragmentNode frag, inout ATFragmentNode list[MAX_NUM_NODES+1])
{
	ATFragmentNode temp, merge;
	float insertFragTrans = unpackColorAlpha(frag.premulColor);

	// Use bubble sort to insert new fragment node (single pass)
	int insertIndex = MAX_NUM_NODES;
	for (int i = 0; i < MAX_NUM_NODES+1; i++) {
		if (frag.depth <= list[i].depth) {
			insertIndex = min(insertIndex, i);

            // Swap
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    // TODO: This code is good for unrolling.
    // However, "int i = insertIndex+1" could be faster => testing!
    for (int i = 0; i < MAX_NUM_NODES+1; i++) {
        if (i > insertIndex) {
            // Update transmittance curve
            float oldTrans = unpackColorAlpha(list[i].premulColor);
            updatePackedColorAlpha(list[i].premulColor, oldTrans*insertFragTrans);
        }
    }
    // TODO: Remove insertFragTrans?
    float parentTrans = insertIndex != 0 ? unpackColorAlpha(list[insertIndex-1].premulColor) : 1.0;
    updatePackedColorAlpha(list[insertIndex].premulColor, parentTrans*insertFragTrans);

    // Merge nodes with least impact to area under transmittance graph
    if (list[MAX_NUM_NODES].depth != DISTANCE_INFINITE) {
        // Code adapted from https://github.com/GameTechDev/AOIT-Update/blob/master/OIT_DX11/AOIT%20Technique/DXAOIT.hlsl
        const int startRemovalIdx = 1;
        float nodeUnderError = 1e30;
        int smallestErrorIndex = MAX_NUM_NODES;
        for (i = startRemovalIdx; i < MAX_NUM_NODES; ++i) {
            // Area under section of graph
            nodeUnderError = (list[i].depth - list[i-1].depth)
                    * (unpackColorAlpha(list[i-1].premulColor)
                        - unpackColorAlpha(list[i].premulColor));
        }

        // Merge nodes
        for (i = startRemovalIdx; i < OIT_NODE_COUNT; ++i) {
            ;
        }


        vec4 src = unpackColorRGBA(list[MAX_NUM_NODES-1].premulColor);
        vec4 dst = unpackColorRGBA(list[MAX_NUM_NODES].premulColor);
        vec4 mergedColor;
        mergedColor.rgb = src.rgb + dst.rgb * src.a;
        mergedColor.a = src.a * dst.a; // Transmittance
        merge.premulColor = packColorRGBA(mergedColor);
        merge.depth = list[MAX_NUM_NODES-1].depth;
        list[MAX_NUM_NODES-1] = merge;
	}
}


void gatherFragment(vec4 color)
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	uint offset = MAX_NUM_NODES*pixelIndex;

	ATFragmentNode frag;
	frag.depth = gl_FragCoord.z;
	// TODO: Remove "*color.a"?
	frag.premulColor = packColorRGBA(vec4(color.rgb * color.a, 1.0 - color.a));


	// Read data from SSBO
	ATFragmentNode nodeArray[MAX_NUM_NODES+1];
	loadFragmentNodes(pixelIndex, nodeArray);

	// Insert node to list (or merge nodes with least impact on transmittance graph)
	adaptiveTransparencyBlending(frag, nodeArray);

	// Write back data to SSBO
	storeFragmentNodes(pixelIndex, nodeArray);

	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}

