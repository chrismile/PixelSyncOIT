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

// States how many fragments are rendered at each location.
// Size: viewportW * viewportH.
layout (std430, binding = 1) coherent buffer NumFragmentsBuffer
{
	uint numFragmentsBuffer[];
};

uniform int viewportW;

uint addrGen(uvec2 addr2D)
{
	return addr2D.x + viewportW * addr2D.y;
}
