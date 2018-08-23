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

// A fragment node stores rendering information about one specific fragment
struct FragmentNode
{
	// RGBA color of the node
	uint color;
	// Depth value of the fragment (in view space)
	float depth;
};

// Stores viewportW * viewportH * nodesPerPixel fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) buffer FragmentNodes
{
	FragmentNode nodes[];
};

// States how many fragment nodes are stored in the nodes buffer for each pixel.
// Size: viewportW * viewportH.
layout (std430, binding = 1) buffer NumFragmentsBuffer
{
	uint numFragmentsBuffer[];
};

// Number of transparent pixels we can store per node
uniform int nodesPerPixel;

uniform int viewportW;
//uniform int viewportH; // Not needed


#define ADDRESSING_TILED

// Address 1D structured buffers as tiled to better data exploit locality
// "OIT to Volumetric Shadow Mapping, 101 Uses for Raster Ordered Views using DirectX 12",
// by Leigh Davies (Intel), March 05, 2015
uint addrGen(uvec2 addr2D)
{
#ifdef ADDRESSING_TILED
	uint surfaceWidth = viewportW >> 1U;
	uvec2 tileAddr2D = addr2D >> 1U;
	uint tileAddr1D = (tileAddr2D.x + surfaceWidth * tileAddr2D.y) << 2U;
	uvec2 pixelAddr2D = addr2D & 0x1U;
	uint pixelAddr1D = (pixelAddr2D.x << 1U) + pixelAddr2D.y;
	return tileAddr1D | pixelAddr1D;
#else
	return addr2D.x + viewportW * addr2D.y;
#endif
}