
#include "MLABHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAddress.glsl"

#define REQUIRE_INVOCATION_INTERLOCK

out vec4 fragColor;

// Adapted version of "Multi-Layer Alpha Blending" [Salvi and Vaidyanathan 2014]
void multiLayerAlphaBlending(in MLABFragmentNode frag, inout MLABFragmentNode list[MAX_NUM_NODES+1])
{
	MLABFragmentNode temp, merge;
	int minOpacityIndex = 0;
	float minOpacity = 1.0;
	// Use bubble sort to insert new fragment node (single pass)
	for (int i = 0; i < MAX_NUM_NODES+1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}

		float fragOpacity = unpackColorAlpha(list[i].premulColor);
        if (fragOpacity < minOpacity) {
            minOpacityIndex = i;
            minOpacity = fragOpacity;
        }
	}

    // Merge last two nodes if necessary
    if (list[MAX_NUM_NODES].depth != DISTANCE_INFINITE) {
        int mergeIndex0 = minOpacityIndex;
        int mergeIndex1 = minOpacityIndex;

		float fragOpacity = unpackColorAlpha(list[i].premulColor);
        if (minOpacityIndex == 0) {
            // Merge first two nodes
            mergeIndex1 += 1;
        } else if (minOpacityIndex == MAX_NUM_NODES) {
            // Merge last two nodes
            mergeIndex0 -= 1;
        } else {
            float fragOpacityLast = unpackColorAlpha(list[minOpacityIndex-1].premulColor);
            float fragOpacityNext = unpackColorAlpha(list[minOpacityIndex+1].premulColor);
            if (fragOpacityLast < fragOpacityNext) {
                mergeIndex0 -= 1;
            } else {
                mergeIndex1 += 1;
            }
        }

        vec4 src = unpackUnorm4x8(list[mergeIndex0].premulColor);
        vec4 dst = unpackUnorm4x8(list[mergeIndex1].premulColor);
        vec4 mergedColor;
        mergedColor.rgb = src.rgb + dst.rgb * src.a;
        mergedColor.a = src.a * dst.a; // Transmittance
        merge.premulColor = packUnorm4x8(mergedColor);
        merge.depth = list[mergeIndex0].depth;
        list[mergeIndex0] = merge;

        // Move back
        for (int i = mergeIndex1; i < MAX_NUM_NODES; i++) {
            list[i] = list[i+1];
        }
	}
}


void gatherFragment(vec4 color)
{
    if (color.a < 0.001) {
        discard;
    }

	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	MLABFragmentNode frag;
	frag.depth = gl_FragCoord.z;
	frag.premulColor = packUnorm4x8(vec4(color.rgb * color.a, 1.0 - color.a));

    // Begin of actual algorithm code
	MLABFragmentNode nodeArray[MAX_NUM_NODES+1];

	// Read data from SSBO
	memoryBarrierBuffer();
	loadFragmentNodes(pixelIndex, nodeArray);

	// Insert node to list (or merge last two nodes)
	multiLayerAlphaBlending(frag, nodeArray);

	// Write back data to SSBO
	storeFragmentNodes(pixelIndex, nodeArray);

	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}

