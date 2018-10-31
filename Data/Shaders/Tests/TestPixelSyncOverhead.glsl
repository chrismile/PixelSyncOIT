-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}

-- Fragment

#version 430 core

#ifdef TEST_PIXEL_SYNC

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_fragment_shader_interlock.txt
#extension GL_ARB_fragment_shader_interlock : require

#ifdef PIXEL_SYNC_ORDERED
layout(pixel_interlock_ordered) in;
#else
layout(pixel_interlock_unordered) in;
#endif

#else
#endif

#ifdef ABC_TEST
//#extension GL_NV_shader_atomic_float : require
#endif


uniform int viewportW;
layout (std430, binding = 0) coherent buffer DataBuffer
{
	uint data[];
};

float doSomeComputation(float x) {
	return log(2*exp(x) + 1.0);
}

#include "TiledAddress.glsl"

layout(pixel_center_integer) in vec4 gl_FragCoord;

#if defined(TEST_COMPUTE)
void main()
{
	uint idx = addrGen(uvec2(uint(gl_FragCoord.x), uint(gl_FragCoord.y)));
#ifdef TEST_PIXEL_SYNC
	// Code A - Pixel Sync
    beginInvocationInterlockARB();
    float oldValue = data[idx];
    float newValue = doSomeComputation(oldValue);
    data[idx] = int(newValue);
    endInvocationInterlockARB();
#else
    // Code B - Atomic Operations
    float oldValue = data[idx];
    float newValue = doSomeComputation(oldValue);
    atomicExchange(data[idx], int(newValue));
#endif
}

#elif defined(TEST_SUM)

void main()
{
	uint idx = addrGen(uvec2(uint(gl_FragCoord.x), uint(gl_FragCoord.y)));
#ifdef TEST_PIXEL_SYNC
	// Code A - Pixel Sync
    beginInvocationInterlockARB();
    data[idx] += 1;
    endInvocationInterlockARB();
#else
    // Code B - Atomic Operations
    atomicAdd(data[idx], 1);
#endif
}

#endif
