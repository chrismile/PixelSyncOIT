// Maximum transmittance deviation for splitting
// TODO: Idea - depending on bucket number
#define TRANSMITTANCE_THRESHOLD 0.1
// Empty node
#define EMPTY_NODE MLABBucketFragmentNode(DISTANCE_INFINITE, 0xFF000000u)


float getMinDepthOfBucket(in uint pixelIndex, in ivec2 fragPos2D, in int bucketIndex) {
#if (NODES_PER_BUCKET % 4 == 0) && (NODES_PER_BUCKET > 4)
    return nodes[pixelIndex*NUM_BUCKETS+bucketIndex].depth[0][0];
#else
    return nodes[pixelIndex*NUM_BUCKETS+bucketIndex].depth[0];
#endif
}

int getBucketIndex(in uint pixelIndex, in ivec2 fragPos2D, in float depth, in int numBucketsUsed)
{
	for (int bucketIndex = 1; bucketIndex < numBucketsUsed; bucketIndex++) {
	    // Bucket found or first bucket empty
		if (depth <= getMinDepthOfBucket(pixelIndex, fragPos2D, bucketIndex)) {
			return bucketIndex-1;
		}
	}
	return numBucketsUsed-1;
}

// splitNodeIndex is start of new bucket
void splitBucket(in uint pixelIndex, in ivec2 fragPos2D, in int bucketIndex, in int splitNodeIndex,
        in uint numBucketsUsed, inout MLABBucketFragmentNode bucketNodes[NODES_PER_BUCKET+1])
{
	int nextBucketIndex = bucketIndex+1;
	MLABBucketFragmentNode nextBucketNodes[NODES_PER_BUCKET+1];

	// 1. Shift buckets behind "bucketIndex" back by one
	for (int i = int(numBucketsUsed)-1; i > bucketIndex; i--) {
		nodes[pixelIndex*NUM_BUCKETS+uint(i)+1u] = nodes[pixelIndex*NUM_BUCKETS+uint(i)];
	}

	// 2. Copy nodes starting with "splitNodeIndex" to "nextBucketIndex"
	// 2.1 Copy the nodes
	for (int i = splitNodeIndex; i < NODES_PER_BUCKET+1; i++) {
		nextBucketNodes[i-splitNodeIndex] = bucketNodes[i];
		bucketNodes[i] = EMPTY_NODE;
	}
	// 2.2 Initialize rest with the clear value
	for (int i = NODES_PER_BUCKET+1-splitNodeIndex; i < NODES_PER_BUCKET+1; i++) {
		nextBucketNodes[i] = EMPTY_NODE;
	}
	// 2.3 Store result to memory
	storeFragmentNodesBucket(pixelIndex, fragPos2D, nextBucketIndex, nextBucketNodes);

	// 3. Compute the new total transmittance
	// 3.1 ... of the bucket at bucketIndex.
	float bucketTransmittance = 1.0, nextBucketTransmittance = 1.0;
	for (int i = 0; i < splitNodeIndex; i++) {
		bucketTransmittance *= unpackColorAlpha(bucketNodes[i].premulColor);
	}
	for (int i = splitNodeIndex; i < NODES_PER_BUCKET+1; i++) {
		nextBucketTransmittance *= unpackColorAlpha(nextBucketNodes[i-splitNodeIndex].premulColor);
	}
	imageStore(transmittanceTexture, ivec3(fragPos2D, bucketIndex), vec4(bucketTransmittance));
	imageStore(transmittanceTexture, ivec3(fragPos2D, nextBucketIndex), vec4(nextBucketTransmittance));
}


bool insertToBucketTransmittance(in MLABBucketFragmentNode frag, inout MLABBucketFragmentNode list[NODES_PER_BUCKET+1],
        out int insertionIndex)
{
    /*float fragTransmittance = unpackUnorm4x8(frag.premulColor).a;
    insertionIndex = NODES_PER_BUCKET+1;
	MLABBucketFragmentNode temp;

	// Use single pass bubble sort to insert new fragment node
	for (int i = 0; i < NODES_PER_BUCKET+1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
			if (insertionIndex > NODES_PER_BUCKET) {
                insertionIndex = i;
			}
		}
	}

    if (insertionIndex > 0) {
        vec4 color = unpackUnorm4x8(list[insertionIndex].premulColor);
        vec4 lastColor = unpackUnorm4x8(list[insertionIndex-1].premulColor);
        color.a *= lastColor.a;
        list[insertionIndex].premulColor = packUnorm4x8(color);
	}

	for (int i = 0; i < NODES_PER_BUCKET+1; i++) {
		if (i > insertionIndex) {
		    // Multiply with new transmittance of new fragment node
            vec4 color = unpackUnorm4x8(list[i].premulColor);
            color.a *= fragTransmittance;
            list[i].premulColor = packUnorm4x8(color);
		}
	}

    // Bucket full?
    return list[NODES_PER_BUCKET].depth != DISTANCE_INFINITE;*/

    MLABBucketFragmentNode temp;
    insertionIndex = NODES_PER_BUCKET+1;

    // Use single pass bubble sort to insert new fragment node
	for (int i = 0; i < NODES_PER_BUCKET+1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
			if (insertionIndex > NODES_PER_BUCKET) {
                insertionIndex = i;
			}
		}
	}

    // Bucket full?
    return list[NODES_PER_BUCKET].depth != DISTANCE_INFINITE;
}

void mergeLastTwoNodesInBucket(inout MLABBucketFragmentNode list[NODES_PER_BUCKET+1])
{
	/*MLABBucketFragmentNode merge;
    vec4 src = unpackUnorm4x8(list[NODES_PER_BUCKET-1].premulColor);
    vec4 dst = unpackUnorm4x8(list[NODES_PER_BUCKET].premulColor);
    vec4 mergedColor;
    #if NODES_PER_BUCKET > 1
    mergedColor.rgb = src.rgb + dst.rgb * (src.a / unpackUnorm4x8(list[NODES_PER_BUCKET-2].premulColor).a);
    #else
    mergedColor.rgb = src.rgb + dst.rgb * src.a;
    #endif
    mergedColor.a = dst.a; // Transmittance
    merge.premulColor = packUnorm4x8(mergedColor);
    merge.depth = list[NODES_PER_BUCKET-1].depth;
    list[NODES_PER_BUCKET-1] = merge;*/

    MLABBucketFragmentNode merge;
    vec4 src = unpackUnorm4x8(list[NODES_PER_BUCKET-1].premulColor);
    vec4 dst = unpackUnorm4x8(list[NODES_PER_BUCKET].premulColor);
    vec4 mergedColor;
    mergedColor.rgb = src.rgb + dst.rgb * src.a;
    mergedColor.a = src.a * dst.a; // Transmittance
    merge.premulColor = packUnorm4x8(mergedColor);
    merge.depth = list[NODES_PER_BUCKET-1].depth;
    list[NODES_PER_BUCKET-1] = merge;
}
