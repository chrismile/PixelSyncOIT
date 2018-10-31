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

// Distance of infinitely far away fragments (used for initialization)
#define DISTANCE_INFINITE (1E30)

// Data structure for MAX_NUM_NODES nodes, packed in vectors for faster access
#if MAX_NUM_NODES == 1
struct MLABFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	float depth[1];
	// RGB color (3 bytes), opacity (1 byte)
	uint premulColor[1];
};
#elif MAX_NUM_NODES == 2
struct MLABFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec2 depth;
	// RGB color (3 bytes), opacity (1 byte)
	uvec2 premulColor;
};
#elif MAX_NUM_NODES == 4
struct MLABFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth;
	// RGB color (3 bytes), opacity (1 byte)
	uvec4 premulColor;
};
#elif MAX_NUM_NODES % 4 == 0
struct MLABFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	vec4 depth[MAX_NUM_NODES/4];
	// RGB color (3 bytes), opacity (1 byte)
	uvec4 premulColor[MAX_NUM_NODES/4];
};
#else
struct MLABFragmentNode_compressed
{
	// Linear depth, i.e. distance to viewer
	float depth[MAX_NUM_NODES];
	// RGB color (3 bytes), opacity (1 byte)
	uint premulColor[MAX_NUM_NODES];
};
#endif

struct MLABFragmentNode
{
	// Linear depth, i.e. distance to viewer
	float depth;
	// RGB color (3 bytes), opacity (1 byte)
	uint premulColor;
};

// Stores viewportW * viewportH * MAX_NUM_NODES fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) coherent buffer FragmentNodes
{
	MLABFragmentNode_compressed nodes[];
};



// Load the fragments into "nodeArray"
void loadFragmentNodes(in uint pixelIndex, out MLABFragmentNode nodeArray[MAX_NUM_NODES+1]) {
    MLABFragmentNode_compressed fragmentNode = nodes[pixelIndex];

#if (MAX_NUM_NODES % 4 == 0) && (MAX_NUM_NODES > 4)
    for (int i = 0; i < MAX_NUM_NODES/4; i++) {
        for (int j = 0; j < 4; j++) {
            MLABFragmentNode node = { fragmentNode.depth[i][j], fragmentNode.premulColor[i][j] };
            nodeArray[i*4 + j] = node;
        }
    }
#else
    for (int i = 0; i < MAX_NUM_NODES; i++) {
        MLABFragmentNode node = { fragmentNode.depth[i], fragmentNode.premulColor[i] };
        nodeArray[i] = node;
    }
#endif

    // For merging to see if last node is unused
    nodeArray[MAX_NUM_NODES].depth = DISTANCE_INFINITE;
}

// Store the fragments from "nodeArray" into VRAM
void storeFragmentNodes(in uint pixelIndex, in MLABFragmentNode nodeArray[MAX_NUM_NODES+1]) {
    MLABFragmentNode_compressed fragmentNode;

#if (MAX_NUM_NODES % 4 == 0) && (MAX_NUM_NODES > 4)
    for (int i = 0; i < MAX_NUM_NODES/4; i++) {
        for(int j = 0; j < 4; j++) {
            fragmentNode.depth[i][j] = nodeArray[4*i + j].depth;
            fragmentNode.premulColor[i][j] = nodeArray[4*i + j].premulColor;
        }
    }
#else
    for (int i = 0; i < MAX_NUM_NODES; i++) {
        fragmentNode.depth[i] = nodeArray[i].depth;
        fragmentNode.premulColor[i] = nodeArray[i].premulColor;
    }
#endif

    nodes[pixelIndex] = fragmentNode;
}

// Reset the data for the passed fragment position
void clearPixel(uint pixelIndex)
{
    // TODO: Compressed?
    MLABFragmentNode nodeArray[MAX_NUM_NODES+1];
    for (uint i = 0; i < MAX_NUM_NODES; i++) {
        nodeArray[i].depth = DISTANCE_INFINITE;
        nodeArray[i].premulColor = 0xFF000000u; // 100% transparency, i.e. 0% opacity
    }
    storeFragmentNodes(pixelIndex, nodeArray);
}