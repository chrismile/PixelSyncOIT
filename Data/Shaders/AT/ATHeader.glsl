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

struct MLABFragmentNode_compressed
{
	// RGB color (3 bytes), translucency (1 byte)
	uint premulColor;
	// Linear depth, i.e. distance to viewer
	float depth;
};

struct MLABFragmentNode
{
	// RGB color, premul. alpha
	vec4 premulColor;
	//float translucency; // Stored in alpha channel
	// Linear depth, i.e. distance to viewer
	float depth;
};

// Stores viewportW * viewportH * nodesPerPixel fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) buffer FragmentNodes
{
	MLABFragmentNode_compressed nodes[];
};

// States how many fragment nodes are stored in the nodes buffer for each pixel.
// Size: viewportW * viewportH.
layout (std430, binding = 1) buffer NumFragmentsBuffer
{
	uint numFragmentsBuffer[];
};


void unpackFragmentNode(in MLABFragmentNode_compressed inputNode, out MLABFragmentNode outputNode)
{
    outputNode.premulColor = unpackColorRGBA(inputNode.premulColor);
    outputNode.depth = inputNode.depth;
}

void packFragmentNode(in MLABFragmentNode inputNode, out MLABFragmentNode_compressed outputNode)
{
    outputNode.premulColor = packColorRGBA(inputNode.premulColor);
    outputNode.depth = inputNode.depth;
}

uniform int viewportW;
