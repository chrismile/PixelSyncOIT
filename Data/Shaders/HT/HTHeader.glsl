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
struct HTFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	float depth[1];
	// RGB color (3 bytes), translucency (1 byte)
	uint premulColor[1];
};
#elif MAX_NUM_NODES == 2
struct HTFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec2 depth;
	// RGB color (3 bytes), translucency (1 byte)
	uvec2 premulColor;
};
#elif MAX_NUM_NODES == 4
struct HTFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth;
	// RGB color (3 bytes), translucency (1 byte)
	uvec4 premulColor;
};
#elif MAX_NUM_NODES == 8
struct HTFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth[2];
	// RGB color (3 bytes), translucency (1 byte)
	uvec4 premulColor[2];
};
#else
#endif

struct HTFragmentNode
{
	// Linear depth, i.e. distance to viewer
	float depth;
	// RGB color (3 bytes), translucency (1 byte)
	uint premulColor;
};


struct HTFragmentTail_compressed
{
	// Accumulated alpha (16 bit) and fragment count (16 bit)
	uint accumAlphaAndCount;
	// RGB Color (30 bit, i.e. 10 bits per component)
	uint accumColor;
};
struct HTFragmentTail
{
	// Accumulated alpha (16 bit) and fragment count (16 bit)
	uint accumFragCount;
	// RGB Color (30 bit, i.e. 10 bits per component) and accumulated alpha
	vec4 accumColor;
};

// Stores viewportW * viewportH * nodesPerPixel fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) buffer FragmentNodes
{
	HTFragmentNode_compressed nodes[];
};
layout (std430, binding = 1) buffer FragmentTails
{
	HTFragmentTail_compressed tails[];
};



// Load the fragments into "nodeArray"
void loadFragmentNodes(in uint pixelIndex, out HTFragmentNode nodeArray[MAX_NUM_NODES]) {
    HTFragmentNode_compressed fragmentNode = nodes[pixelIndex];

#if MAX_NUM_NODES == 8
    // Should be easier to unroll by compiler than adding an outer loop for first/second array
    for(int i = 0; i < 4; i++) {
        HTFragmentNode node = { fragmentNode.depth[0][i], fragmentNode.premulColor[0][i] };
        nodeArray[0 + i] = node;
    }
    for(int i = 0; i < 4; i++) {
        HTFragmentNode node = { fragmentNode.depth[1][i], fragmentNode.premulColor[1][i] };
        nodeArray[4 + i] = node;
    }
#else
    for (int i = 0; i < MAX_NUM_NODES; i++) {
        HTFragmentNode node = { fragmentNode.depth[i], fragmentNode.premulColor[i] };
        nodeArray[i] = node;
    }
#endif

    // For merging to see if last node is unused
    //nodeArray[MAX_NUM_NODES].depth = DISTANCE_INFINITE;
}

// Store the fragments from "nodeArray" into VRAM
void storeFragmentNodes(in uint pixelIndex, in HTFragmentNode nodeArray[MAX_NUM_NODES]) {
    HTFragmentNode_compressed fragmentNode;

#if MAX_NUM_NODES == 8
    for(int i = 0; i < 4; i++) {
        fragmentNode.depth[0][i] =  nodeArray[0 + i].depth;
        fragmentNode.premulColor[0][i] = nodeArray[0 + i].premulColor;
    }
    for(int i = 0; i < 4; i++) {
        fragmentNode.depth[1][i] =  nodeArray[4 + i].depth;
        fragmentNode.premulColor[1][i] = nodeArray[4 + i].premulColor;
    }
#else
    for (int i = 0; i < MAX_NUM_NODES; i++) {
        fragmentNode.depth[i] = nodeArray[i].depth;
        fragmentNode.premulColor[i] = nodeArray[i].premulColor;
    }
#endif

    nodes[pixelIndex] = fragmentNode;
}


// Forward declarations
uint packAccumAlphaAndFragCount(float accumAlpha, uint fragCount);
void unpackAccumAlphaAndFragCount(in uint packedValue, out float accumAlpha, out uint fragCount);
uint packColor30bit(vec4 vecColor);
vec4 unpackColor30bit(uint packedColor);

void unpackFragmentTail(in HTFragmentTail_compressed inputNode, out HTFragmentTail outputNode)
{
    outputNode.accumColor = unpackColor30bit(inputNode.accumColor);
    unpackAccumAlphaAndFragCount(inputNode.accumAlphaAndCount, outputNode.accumColor.a, outputNode.accumFragCount);
}

void packFragmentTail(in HTFragmentTail inputNode, out HTFragmentTail_compressed outputNode)
{
    outputNode.accumColor = packColor30bit(inputNode.accumColor);
    outputNode.accumAlphaAndCount = packAccumAlphaAndFragCount(inputNode.accumColor.a, inputNode.accumFragCount);
}

// Reset the data for the passed fragment position
void clearPixel(uint pixelIndex)
{
    // Clear list
    HTFragmentNode nodeArray[MAX_NUM_NODES];
    for (uint i = 0; i < MAX_NUM_NODES; i++) {
        nodeArray[i].depth = DISTANCE_INFINITE;
        nodeArray[i].premulColor = 0xFF000000u; // 100% translucency, i.e. 0% opacity
    }
    storeFragmentNodes(pixelIndex, nodeArray);

    // Clear accumulation tail
    HTFragmentTail_compressed tail;
    tail.accumColor = 0u;
    tail.accumAlphaAndCount = 0u;
    tails[pixelIndex] = tail;
}

