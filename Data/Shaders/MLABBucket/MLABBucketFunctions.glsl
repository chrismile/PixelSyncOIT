// Max bounding box difference
#define BB_SPLIT_OPACITY_DIFFERENCE 0.4
// Nodes with high opacity will be put first in new split bucket
#define HIGH_OPACITY 0.3
// Empty bounding box (min_depth, max_depth, min_opacity, max_opacity)
//#define EMPTY_BB vec4(1e7, -1e7, 1.0, 0.0)
#define EMPTY_BB vec4(1.0, 0.0, 1.0, 0.0)
#define INITIAL_BB vec4(0.0, 1.0, 1.0, 0.0)
// Empty node
#define EMPTY_NODE MLABBucketFragmentNode(DISTANCE_INFINITE, 0xFF000000u)


int getBucketIndex(in uint pixelIndex, in ivec2 fragPos2D, in float depth, in int numBucketsUsed)
{
	/*for (int bucketIndex = 0; bucketIndex < numBucketsUsed; bucketIndex++) {
	    vec4 boundingBox = imageLoad(boundingBoxesTexture, ivec3(fragPos2D, bucketIndex));
	    // Bucket found or first bucket empty
		if (depth <= boundingBox.y) {
			return bucketIndex;
		}
	}
	return numBucketsUsed-1;*/
	for (int bucketIndex = 1; bucketIndex < numBucketsUsed; bucketIndex++) {
	    vec4 boundingBox = imageLoad(boundingBoxesTexture, ivec3(fragPos2D, bucketIndex));
	    // Bucket found or first bucket empty
		if (depth <= boundingBox.x) {
			return bucketIndex-1;
		}
	}
	return numBucketsUsed-1;
}

// TODO: Search highest opacity instead of first above threshold?
int findFirstNodeWithHighOpacity(in MLABBucketFragmentNode bucketNodes[NODES_PER_BUCKET+1])
{
	for (int nodeIndex = 1; nodeIndex < NODES_PER_BUCKET; nodeIndex++) {
	    float opacity = 1.0 - unpackUnorm4x8(bucketNodes[nodeIndex].premulColor).a; // TODO: Unpack only alpha
		if (opacity > HIGH_OPACITY) {
			return nodeIndex;
		}
	}

	// Split in half if no other index found.
	return (NODES_PER_BUCKET+1)/2;
}



void combineBucketBB(inout vec4 bucketBB, in MLABBucketFragmentNode node)
{
	bucketBB.x = min(bucketBB.x, node.depth);
	bucketBB.y = max(bucketBB.y, node.depth);
	float opacity = 1.0 - unpackUnorm4x8(node.premulColor).a; // TODO: Unpack only alpha
	bucketBB.z = min(bucketBB.z, opacity);
	bucketBB.w = max(bucketBB.w, opacity);
}

// splitNodeIndex is start of new bucket
void splitBucket(in uint pixelIndex, in ivec2 fragPos2D, in int bucketIndex, inout vec4 bucketBB, in int splitNodeIndex,
        in uint numBucketsUsed, inout MLABBucketFragmentNode bucketNodes[NODES_PER_BUCKET+1])
{
	int nextBucketIndex = bucketIndex+1;
	MLABBucketFragmentNode nextBucketNodes[NODES_PER_BUCKET+1];

	// 1. Shift buckets behind "bucketIndex" back by one (including bounding boxes)
	for (int i = int(numBucketsUsed)-1; i > bucketIndex; i--) {
		nodes[pixelIndex*NUM_BUCKETS+uint(i)+1u] = nodes[pixelIndex*NUM_BUCKETS+uint(i)];
        vec4 shiftedBucketBB = imageLoad(boundingBoxesTexture, ivec3(fragPos2D, i));
        imageStore(boundingBoxesTexture, ivec3(fragPos2D, i+1), shiftedBucketBB);
	}

	// 2. Copy nodes starting with "splitNodeIndex" to "nextBucketIndex"
	// 2.1 Copy the nodes (and clear afterwards) and compute the bounding box of the split bucket
	vec4 nextBucketBB = EMPTY_BB;
	for (int i = splitNodeIndex; i < NODES_PER_BUCKET+1; i++) {
		nextBucketNodes[i-splitNodeIndex] = bucketNodes[i];
		bucketNodes[i] = EMPTY_NODE;
		combineBucketBB(nextBucketBB, nextBucketNodes[i-splitNodeIndex]);
	}
	// 2.2 Initialize rest with the clear value
	for (int i = NODES_PER_BUCKET+1-splitNodeIndex; i < NODES_PER_BUCKET+1; i++) {
		nextBucketNodes[i] = EMPTY_NODE;
	}
	// 2.3 Store result to memory
	storeFragmentNodesBucket(pixelIndex, fragPos2D, nextBucketIndex, nextBucketNodes, nextBucketBB);

	// 3. Compute the new bounding box
	bucketBB = EMPTY_BB;
	for (int i = 0; i < splitNodeIndex; i++) {
		combineBucketBB(bucketBB, bucketNodes[i]);
	}
}


bool insertToBucket(in MLABBucketFragmentNode frag, inout MLABBucketFragmentNode list[NODES_PER_BUCKET+1])
{
	MLABBucketFragmentNode temp;
	// Use single pass bubble sort to insert new fragment node
	for (int i = 0; i < NODES_PER_BUCKET+1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    // Bucket full?
    return list[NODES_PER_BUCKET].depth != DISTANCE_INFINITE;
}

void mergeLastTwoNodesInBucket(inout MLABBucketFragmentNode list[NODES_PER_BUCKET+1])
{
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
