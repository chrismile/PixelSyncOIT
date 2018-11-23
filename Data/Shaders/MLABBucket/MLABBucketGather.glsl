
#include "MLABBucketHeader.glsl"
#if defined(MLAB_DEPTH_OPACITY_BUCKETS)
#include "MLABBucketFunctions.glsl"
#elif defined(MLAB_TRANSMITTANCE_BUCKETS)
#include "MLABBucketFunctionsTransmittance.glsl"
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

// Adapted version of "Multi-Layer Alpha Blending" [Salvi and Vaidyanathan 2014]
void multiLayerAlphaBlendingOffset(in MLABBucketFragmentNode frag, inout MLABBucketFragmentNode list[BUFFER_SIZE+1])
{
	MLABBucketFragmentNode temp, merge;
	// Use bubble sort to insert new fragment node (single pass)
	for (int i = 1; i < BUFFER_SIZE+1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    // Merge last two nodes if necessary
    if (list[BUFFER_SIZE].depth != DISTANCE_INFINITE) {
        vec4 src = unpackUnorm4x8(list[BUFFER_SIZE-1].premulColor);
        vec4 dst = unpackUnorm4x8(list[BUFFER_SIZE].premulColor);
        vec4 mergedColor;
        mergedColor.rgb = src.rgb + dst.rgb * src.a;
        mergedColor.a = src.a * dst.a; // Transmittance
        merge.premulColor = packUnorm4x8(mergedColor);
        merge.depth = list[BUFFER_SIZE-1].depth;
        list[BUFFER_SIZE-1] = merge;
	}
}


void multiLayerAlphaBlendingMergeFront(in MLABBucketFragmentNode frag, inout MLABBucketFragmentNode list[BUFFER_SIZE+1])
{
	MLABBucketFragmentNode temp, merge;
	if (frag.depth <= list[0].depth) {
		temp = list[0];
		list[0] = frag;
		frag = temp;
	}

    // Merge last two nodes if necessary
    if (frag.depth != DISTANCE_INFINITE) {
        vec4 src = unpackUnorm4x8(list[0].premulColor);
        vec4 dst = unpackUnorm4x8(frag.premulColor);
        vec4 mergedColor;
        mergedColor.rgb = src.rgb + dst.rgb * src.a;
        mergedColor.a = src.a * dst.a; // Transmittance
        merge.premulColor = packUnorm4x8(mergedColor);
        merge.depth = list[0].depth;
        list[0] = merge;
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

#if defined(MLAB_DEPTH_BUCKETS) || defined(MLAB_OPACITY_BUCKETS)

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


#elif defined(MLAB_DEPTH_OPACITY_BUCKETS)
    uint numBucketsUsed = imageLoad(numUsedBucketsTexture, fragPos2D).r;
	MLABBucketFragmentNode bucketNodes[NODES_PER_BUCKET+1];
	float depth = logDepthWarp(-screenSpacePosition.z);
	frag.depth = depth;
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

#elif defined(MLAB_TRANSMITTANCE_BUCKETS)
    uint numBucketsUsed = imageLoad(numUsedBucketsTexture, fragPos2D).r;
	MLABBucketFragmentNode bucketNodes[NODES_PER_BUCKET+1];
	float depth = logDepthWarp(-screenSpacePosition.z);
	frag.depth = depth;
	int bucketIndex = getBucketIndex(pixelIndex, fragPos2D, depth, int(numBucketsUsed));
    loadFragmentNodesBucket(pixelIndex, fragPos2D, bucketIndex, bucketNodes);
	float oldTransmittance = unpackUnorm4x8(bucketNodes[NODES_PER_BUCKET].premulColor).a;
	int insertionIndex = 0;
    bool tooFull = insertToBucketTransmittance(frag, bucketNodes, insertionIndex); // Without merging
	float newTransmittance = unpackUnorm4x8(bucketNodes[NODES_PER_BUCKET].premulColor).a;
    // Either split bucket or merge bucket content
    if (tooFull) {
    	bool shallMerge = false;

    	if (numBucketsUsed >= NUM_BUCKETS) {
    		// Already maximum number of buckets
    		shallMerge = true;
    	} else {
    		// Split or merge? (depending on maximum opacity difference of nodes in the bucket)
    		float transmittanceDifference = oldTransmittance - newTransmittance;
    		if (transmittanceDifference > TRANSMITTANCE_THRESHOLD && insertionIndex > 0) {
    			splitBucket(pixelIndex, fragPos2D, bucketIndex, insertionIndex, numBucketsUsed, bucketNodes);
    			imageStore(numUsedBucketsTexture, fragPos2D, uvec4(numBucketsUsed+1));
    		} else {
    			shallMerge = true;
    		}
    	}

    	if (shallMerge) {
    		mergeLastTwoNodesInBucket(bucketNodes);
    	}
    }
    storeFragmentNodesBucket(pixelIndex, fragPos2D, bucketIndex, bucketNodes);

#elif defined(MLAB_MIN_DEPTH_BUCKETS)
	MLABBucketFragmentNode nodeArray[BUFFER_SIZE+1];
	loadFragmentNodes(pixelIndex, fragPos2D, nodeArray);
	float depth = logDepthWarp(-screenSpacePosition.z);
    frag.depth = depth;

    //frag.premulColor = packUnorm4x8(vec4(vec3(depth), 0.0));
    //frag.premulColor = packUnorm4x8(vec4(vec3(minDepth[pixelIndex]), 0.0));
    if (depth < minDepth[pixelIndex]) {
        // Merge new fragment with first one
        multiLayerAlphaBlendingMergeFront(frag, nodeArray);
    } else {
        // Insert normally (with offset of one)
        multiLayerAlphaBlendingOffset(frag, nodeArray);
    }
	storeFragmentNodes(pixelIndex, fragPos2D, nodeArray);
#endif


	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}

