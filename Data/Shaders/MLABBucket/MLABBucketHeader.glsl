// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
#extension GL_ARB_shader_image_load_store : require

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_fragment_shader_interlock.txt
#extension GL_ARB_fragment_shader_interlock : require

#pragma optionNV (unroll all)

#ifdef PIXEL_SYNC_ORDERED
// Use early z-test to cull transparent fragments occluded by opaque fragments.
// Additionaly, use fragment interlock.
layout(early_fragment_tests, pixel_interlock_ordered) in;
#else
// Use early z-test to cull transparent fragments occluded by opaque fragments.
// Additionaly, use fragment interlock.
layout(early_fragment_tests, pixel_interlock_unordered) in;
#endif

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer) in vec4 gl_FragCoord;

uniform int viewportW;

uniform float logDepthMin;
uniform float logDepthMax;

// Maps depth to range [0,1] with logarithmic scale
float logDepthWarp(float z)
{
	return (log(z) - logDepthMin) / (logDepthMax - logDepthMin);
	//return (z - exp(logmin)) / (exp(logmax) - exp(logmin));
}

// Distance of infinitely far away fragments (used for initialization)
#define DISTANCE_INFINITE (1E30)

// Data structure for NODES_PER_BUCKET nodes, packed in vectors for faster access
#if NODES_PER_BUCKET == 1
struct MLABBucketFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	float depth[1];
	// RGB color (3 bytes), opacity (1 byte)
	uint premulColor[1];
};
#elif NODES_PER_BUCKET == 2
struct MLABBucketFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec2 depth;
	// RGB color (3 bytes), opacity (1 byte)
	uvec2 premulColor;
};
#elif NODES_PER_BUCKET == 4
struct MLABBucketFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth;
	// RGB color (3 bytes), opacity (1 byte)
	uvec4 premulColor;
};
#elif NODES_PER_BUCKET % 4 == 0
struct MLABBucketFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth[NODES_PER_BUCKET/4];
	// RGB color (3 bytes), opacity (1 byte)
	uvec4 premulColor[NODES_PER_BUCKET/4];
};
#else
struct MLABBucketFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	float depth[NODES_PER_BUCKET];
	// RGB color (3 bytes), opacity (1 byte)
	uint premulColor[NODES_PER_BUCKET];
};
#endif

struct MLABBucketFragmentNode
{
	// Linear depth, i.e. distance to viewer
	float depth;
	// RGB color (3 bytes), opacity (1 byte)
	uint premulColor;
};

// Stores viewportW * viewportH * BUFFER_SIZE fragments.
layout (std430, binding = 0) coherent buffer FragmentNodes
{
	MLABBucketFragmentNode_compressed nodes[];
};



// Load the fragments into "nodeArray"
void loadFragmentNodesBucket(in uint pixelIndex, in int bucketIndex, out MLABBucketFragmentNode nodeArray[NODES_PER_BUCKET+1]) {
    MLABBucketFragmentNode_compressed fragmentNode = nodes[pixelIndex*NUM_BUCKETS+bucketIndex];

#if (NODES_PER_BUCKET % 4 == 0) && (NODES_PER_BUCKET > 4)
    for (int i = 0; i < NODES_PER_BUCKET/4; i++) {
        for (int j = 0; j < 4; j++) {
            MLABBucketFragmentNode node = { fragmentNode.depth[i][j], fragmentNode.premulColor[i][j] };
            nodeArray[i*4 + j] = node;
        }
    }
#else
    for (int i = 0; i < NODES_PER_BUCKET; i++) {
        MLABBucketFragmentNode node = { fragmentNode.depth[i], fragmentNode.premulColor[i] };
        nodeArray[i] = node;
    }
#endif

    // For merging to see if last node is unused
    nodeArray[NODES_PER_BUCKET].depth = DISTANCE_INFINITE;
}

// Store the fragments from "nodeArray" into VRAM
void storeFragmentNodesBucket(in uint pixelIndex, in int bucketIndex, in MLABBucketFragmentNode nodeArray[NODES_PER_BUCKET+1]) {
    MLABBucketFragmentNode_compressed fragmentNode;

#if (NODES_PER_BUCKET % 4 == 0) && (NODES_PER_BUCKET > 4)
    for (int i = 0; i < NODES_PER_BUCKET/4; i++) {
        for(int j = 0; j < 4; j++) {
            fragmentNode.depth[i][j] = nodeArray[4*i + j].depth;
            fragmentNode.premulColor[i][j] = nodeArray[4*i + j].premulColor;
        }
    }
#else
    for (int i = 0; i < NODES_PER_BUCKET; i++) {
        fragmentNode.depth[i] = nodeArray[i].depth;
        fragmentNode.premulColor[i] = nodeArray[i].premulColor;
    }
#endif

    nodes[pixelIndex*NUM_BUCKETS+bucketIndex] = fragmentNode;
}



// Load the fragments into "nodeArray"
void loadFragmentNodes(in uint pixelIndex, out MLABBucketFragmentNode nodeArray[BUFFER_SIZE+1]) {
#if (NODES_PER_BUCKET % 4 == 0) && (NODES_PER_BUCKET > 4)
    for (int bucketIndex = 0; bucketIndex < NUM_BUCKETS; bucketIndex++) {
        MLABBucketFragmentNode_compressed fragmentNode = nodes[pixelIndex*NUM_BUCKETS+bucketIndex];
        for (int i = 0; i < NODES_PER_BUCKET/4; i++) {
            for (int j = 0; j < 4; j++) {
                MLABBucketFragmentNode node = { fragmentNode.depth[i][j], fragmentNode.premulColor[i][j] };
                nodeArray[bucketIndex*NODES_PER_BUCKET + i*4 + j] = node;
            }
        }
    }
#else
    for (int bucketIndex = 0; bucketIndex < NUM_BUCKETS; bucketIndex++) {
        MLABBucketFragmentNode_compressed fragmentNode = nodes[pixelIndex*NUM_BUCKETS+bucketIndex];
        for (int i = 0; i < NODES_PER_BUCKET; i++) {
            MLABBucketFragmentNode node = { fragmentNode.depth[i], fragmentNode.premulColor[i] };
            nodeArray[bucketIndex*NODES_PER_BUCKET + i] = node;
        }
    }
#endif

    // For merging to see if last node is unused
    nodeArray[BUFFER_SIZE].depth = DISTANCE_INFINITE;
}

// Store the fragments from "nodeArray" into VRAM
void storeFragmentNodes(in uint pixelIndex, in MLABBucketFragmentNode nodeArray[BUFFER_SIZE+1]) {
    MLABBucketFragmentNode_compressed fragmentNode;

#if (NODES_PER_BUCKET % 4 == 0) && (NODES_PER_BUCKET > 4)
    for (int bucketIndex = 0; bucketIndex < NUM_BUCKETS; bucketIndex++) {
        for (int i = 0; i < NODES_PER_BUCKET/4; i++) {
            for(int j = 0; j < 4; j++) {
                fragmentNode.depth[i][j] = nodeArray[4*i + j].depth;
                fragmentNode.premulColor[i][j] = nodeArray[4*i + j].premulColor;
            }
        }
        nodes[pixelIndex*NUM_BUCKETS+bucketIndex] = fragmentNode;
    }
#else
    for (int bucketIndex = 0; bucketIndex < NUM_BUCKETS; bucketIndex++) {
        for (int i = 0; i < NODES_PER_BUCKET; i++) {
            fragmentNode.depth[i] = nodeArray[i].depth;
            fragmentNode.premulColor[i] = nodeArray[i].premulColor;
        }
        nodes[pixelIndex*NUM_BUCKETS+bucketIndex] = fragmentNode;
    }
#endif
}


// Reset the data for the passed fragment position
void clearPixel(uint pixelIndex)
{
    MLABBucketFragmentNode nodeArray[BUFFER_SIZE+1];
    for (uint i = 0; i < BUFFER_SIZE; i++) {
        nodeArray[i].depth = DISTANCE_INFINITE;
        nodeArray[i].premulColor = 0xFF000000u; // 100% transparency, i.e. 0% opacity
    }
    storeFragmentNodes(pixelIndex, nodeArray);
}
