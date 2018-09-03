// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
#extension GL_ARB_shader_image_load_store : require

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_fragment_shader_interlock.txt
#extension GL_ARB_fragment_shader_interlock : require

#pragma optionNV (unroll all)

// Use early z-test to cull transparent fragments occluded by opaque fragments.
// Additionaly, use fragment interlock.
layout(early_fragment_tests, pixel_interlock_unordered) in;

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer) in vec4 gl_FragCoord;

uniform int viewportW;

#define MAX_NUM_NODES 4

// Distance of infinitely far away fragments (used for initialization)
#define DISTANCE_INFINITE (1E30)

// Data structure for 4 nodes, packed in vectors for faster access
#if MAX_NUM_NODES == 1
struct ATFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	float depth[1];
	// Transmittance of the fragment
	float transmittance[1];
};
#elif MAX_NUM_NODES == 2
struct ATFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec2 depth;
	// Transmittance of the fragment
	vec2 transmittance;
};
#elif MAX_NUM_NODES == 4
struct ATFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth;
	// Transmittance of the fragment
	vec4 transmittance;
};
#elif MAX_NUM_NODES == 8
struct ATFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth[2];
	// Transmittance of the fragment
	vec4 transmittance[2];
};
#else
#endif

struct ATFragmentNode
{
	// Linear depth, i.e. distance to viewer
	float depth;
	// Transmittance of the fragment
	float transmittance;
};

// Stores viewportW * viewportH * nodesPerPixel fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) buffer FragmentNodes
{
	ATFragmentNode_compressed nodes[];
};



// Load the fragments into "nodeArray"
void loadFragmentNodes(in uint pixelIndex, out ATFragmentNode nodeArray[MAX_NUM_NODES+1]) {
    ATFragmentNode_compressed fragmentNode = nodes[pixelIndex];

#if MAX_NUM_NODES == 8
    // Should be easier to unroll by compiler than adding an outer loop for first/second array
    for(int i = 0; i < 4; i++) {
        ATFragmentNode node = { fragmentNode.depth[0][i], fragmentNode.transmittance[0][i] };
        nodeArray[0 + i] = node;
    }
    for(int i = 0; i < 4; i++) {
        ATFragmentNode node = { fragmentNode.depth[1][i], fragmentNode.transmittance[1][i] };
        nodeArray[4 + i] = node;
    }
#else
    for (int i = 0; i < MAX_NUM_NODES; i++) {
        ATFragmentNode node = { fragmentNode.depth[i], fragmentNode.transmittance[i] };
        nodeArray[i] = node;
    }
#endif

    // For merging to see if last node is unused
    nodeArray[MAX_NUM_NODES].depth = DISTANCE_INFINITE;
}

// Store the fragments from "nodeArray" into VRAM
void storeFragmentNodes(in uint pixelIndex, in ATFragmentNode nodeArray[MAX_NUM_NODES+1]) {
    ATFragmentNode_compressed fragmentNode;

#if MAX_NUM_NODES == 8
    for(int i = 0; i < 4; i++) {
        fragmentNode.depth[0][i] =  nodeArray[0 + i].depth;
        fragmentNode.transmittance[0][i] = nodeArray[0 + i].transmittance;
    }
    for(int i = 0; i < 4; i++) {
        fragmentNode.depth[1][i] =  nodeArray[4 + i].depth;
        fragmentNode.transmittance[1][i] = nodeArray[4 + i].transmittance;
    }
#else
    for (int i = 0; i < MAX_NUM_NODES; i++) {
        fragmentNode.depth[i] = nodeArray[i].depth;
        fragmentNode.transmittance[i] = nodeArray[i].transmittance;
    }
#endif

    nodes[pixelIndex] = fragmentNode;
}

// Reset the data for the passed fragment position
void clearNode(uint pixelIndex)
{
    // TODO: Compressed?
    ATFragmentNode nodeArray[MAX_NUM_NODES+1];
    for (uint i = 0; i < MAX_NUM_NODES; i++) {
        nodeArray[i].depth = DISTANCE_INFINITE;
        nodeArray[i].transmittance = 1.0; // 100% transmittance
    }
    storeFragmentNodes(pixelIndex, nodeArray);
}