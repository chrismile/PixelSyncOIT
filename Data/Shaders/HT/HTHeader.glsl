// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
#extension GL_ARB_shader_image_load_store : require

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_fragment_shader_interlock.txt
#extension GL_ARB_fragment_shader_interlock : require

// Use early z-test to cull transparent fragments occluded by opaque fragments.
// Additionaly, use fragment interlock.
layout(early_fragment_tests, pixel_interlock_unordered) in;

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer) in vec4 gl_FragCoord;

#define MAX_NUM_NODES 8

struct HTFragmentNode
{
	// RGB color (3 bytes), translucency (1 byte)
	vec4 premulColor;
	// Linear depth, i.e. distance to viewer
	float depth;
};

struct HTFragmentTail
{
	// RGB Color (30 bit, i.e. 10 bits per component) and accumulated alpha
	vec4 accumColor;
	// Accumulated alpha (16 bit) and fragment count (16 bit)
	//float accumAlpha;
	uint accumFragCount;
};

struct HTFragmentNode_compressed
{
	// RGB color (3 bytes), translucency (1 byte)
	uint premulColor;
	// Linear depth, i.e. distance to viewer
	float depth;
};

struct HTFragmentTail_compressed
{
	// RGB Color (30 bit, i.e. 10 bits per component)
	uint accumColor;
	// Accumulated alpha (16 bit) and fragment count (16 bit)
	uint accumAlphaAndCount;
};

struct HTFragmentList_compressed
{
	HTFragmentNode_compressed nodes[MAX_NUM_NODES];
	HTFragmentTail_compressed tail;
};

// Stores viewportW * viewportH * nodesPerPixel fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) buffer FragmentNodes
{
	HTFragmentList_compressed nodes[];
};

// States how many fragment nodes are stored in the nodes buffer for each pixel.
// Size: viewportW * viewportH.
layout (std430, binding = 1) buffer NumFragmentsBuffer
{
	uint numFragmentsBuffer[];
};


void unpackFragmentNode(in HTFragmentNode_compressed inputNode, out HTFragmentNode outputNode)
{
    outputNode.premulColor = unpackColorRGBA(inputNode.premulColor);
    outputNode.depth = inputNode.depth;
}

void packFragmentNode(in HTFragmentNode inputNode, out HTFragmentNode_compressed outputNode)
{
    outputNode.premulColor = packColorRGBA(inputNode.premulColor);
    outputNode.depth = inputNode.depth;
}


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

uniform int viewportW;
