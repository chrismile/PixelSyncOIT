
#include "MLABBucketHeader.glsl"
#ifdef MLAB_DEPTH_OPACITY_BUCKETS
#include "MLABBucketFunctions.glsl"
#endif
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

    ivec2 fragPos2D = ivec2(int(gl_FragCoord.x), int(gl_FragCoord.y));
	uint pixelIndex = addrGen(uvec2(fragPos2D));

	MLABBucketFragmentNode frag;
	frag.depth = gl_FragCoord.z;
	frag.premulColor = packUnorm4x8(vec4(color.rgb * color.a, 1.0 - color.a));

#ifndef MLAB_DEPTH_OPACITY_BUCKETS

#if defined(MLAB_DEPTH_BUCKETS)
	float depth = logDepthWarp(-screenSpacePosition.z);
    int bucketIndex = int(floor(depth * float(NUM_BUCKETS)));
#elif defined(MLAB_OPACITY_BUCKETS)
    int bucketIndex = int(floor(color.a * float(NUM_BUCKETS)));
#endif

    // Begin of actual algorithm code
	MLABBucketFragmentNode nodeArray[NODES_PER_BUCKET+1];

	// Read data from SSBO
	memoryBarrierBuffer();
	loadFragmentNodesBucket(pixelIndex, fragPos2D, bucketIndex, nodeArray);

	// Insert node to list (or merge last two nodes)
	multiLayerAlphaBlending(frag, nodeArray);

	// Write back data to SSBO
	storeFragmentNodesBucket(pixelIndex, fragPos2D, bucketIndex, nodeArray);


#else
    uint numBucketsUsed = imageLoad(numUsedBucketsTexture, fragPos2D).r;
	MLABBucketFragmentNode bucketNodes[NODES_PER_BUCKET+1];
	float depth = logDepthWarp(-screenSpacePosition.z);
	int bucketIndex = getBucketIndex(pixelIndex, fragPos2D, depth, int(numBucketsUsed));
    vec4 bucketBB;
    loadFragmentNodesBucket(pixelIndex, fragPos2D, bucketIndex, bucketNodes, bucketBB);
    bool tooFull = insertToBucket(frag, bucketNodes); // Without merging
    combineBucketBB(bucketBB, frag);
    // Either split bucket or merge bucket content
    if (tooFull) {
    	bool shallMerge = false;

    	if (numBucketsUsed >= NUM_BUCKETS) {
    		// Already maximum number of buckets
    		shallMerge = true;
    	} else {
    		// Split or merge? (depending on maximum opacity difference of nodes in the bucket)
    		float bucketMaxOpacityDifference = bucketBB.w - bucketBB.z;
    		//if (bucketMaxOpacityDifference > BB_SPLIT_OPACITY_DIFFERENCE) {
    		if (bucketBB.w > BB_SPLIT_OPACITY_DIFFERENCE) {
    			int splitIndex = findFirstNodeWithHighOpacity(bucketNodes);
    			splitBucket(pixelIndex, fragPos2D, bucketIndex, bucketBB, splitIndex, numBucketsUsed, bucketNodes);
    			imageStore(numUsedBucketsTexture, fragPos2D, uvec4(numBucketsUsed+1));
    		} else {
    			shallMerge = true;
    		}
    	}

    	if (shallMerge) {
    		mergeLastTwoNodesInBucket(bucketNodes);
    	}
    }
    storeFragmentNodesBucket(pixelIndex, fragPos2D, bucketIndex, bucketNodes, bucketBB);
#endif


	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}

