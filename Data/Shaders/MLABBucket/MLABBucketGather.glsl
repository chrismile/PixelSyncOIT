
#include "MLABBucketHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAddress.glsl"

#define REQUIRE_INVOCATION_INTERLOCK

out vec4 fragColor;

// Adapted version of "Multi-Layer Alpha Blending" [Salvi and Vaidyanathan 2014]
void multiLayerAlphaBlending(in MLABBucketFragmentNode frag, inout MLABBucketFragmentNode list[NODES_PER_BUCKET+1])
{
	MLABBucketFragmentNode temp, merge;
	// Use bubble sort to insert new fragment node (single pass)
	for (int i = 0; i < NODES_PER_BUCKET+1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    // Merge last two nodes if necessary
    if (list[NODES_PER_BUCKET].depth != DISTANCE_INFINITE) {
        vec4 src = unpackUnorm4x8(list[NODES_PER_BUCKET-1].premulColor);
        vec4 dst = unpackUnorm4x8(list[NODES_PER_BUCKET].premulColor);
        vec4 mergedColor;
        mergedColor.rgb = src.rgb + dst.rgb * src.a;
        mergedColor.a = src.a * dst.a; // Transmittance
        merge.premulColor = packUnorm4x8(mergedColor);
        merge.depth = list[NODES_PER_BUCKET-1].depth;
        list[NODES_PER_BUCKET-1] = merge;
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

	MLABBucketFragmentNode frag;
	frag.depth = gl_FragCoord.z;
	frag.premulColor = packUnorm4x8(vec4(color.rgb * color.a, 1.0 - color.a));

	float depth = logDepthWarp(-screenSpacePosition.z);
    int bucketIdx = int(floor(depth * float(NUM_BUCKETS)));
   // int bucketIdx = int(floor(color.a * float(NUM_BUCKETS)));

    // Begin of actual algorithm code
	MLABBucketFragmentNode nodeArray[NODES_PER_BUCKET+1];

	// Read data from SSBO
	memoryBarrierBuffer();
	loadFragmentNodesBucket(pixelIndex, bucketIdx, nodeArray);

	// Insert node to list (or merge last two nodes)
	multiLayerAlphaBlending(frag, nodeArray);

	// Write back data to SSBO
	storeFragmentNodesBucket(pixelIndex, bucketIdx, nodeArray);

	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}

