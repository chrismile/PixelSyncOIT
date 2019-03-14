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

#ifdef PIXEL_SYNC_UNORDERED
layout(pixel_interlock_unordered) in;
#else
layout(pixel_interlock_ordered) in;
#endif

#else
#endif

//#extension GL_NV_shader_atomic_float : require


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
#if defined(TEST_PIXEL_SYNC)
    // Code A - Pixel Sync
    beginInvocationInterlockARB();
    float value = float(data[idx]);
    for (int i = 0; i < 10; i++) {
        value = doSomeComputation(value);
    }
    data[idx] = int(value);
    endInvocationInterlockARB();
#elif defined(TEST_ATOMIC_OPERATIONS)
    // Code B - Atomic Operations
    float value = float(data[idx]);
    for (int i = 0; i < 10; i++) {
        value = doSomeComputation(value);
    }
    atomicExchange(data[idx], int(value));
#else
    // Code C - No Synchronization
    float value = float(data[idx]);
    for (int i = 0; i < 10; i++) {
        value = doSomeComputation(value);
    }
    data[idx] = int(value);
#endif
}

#elif defined(TEST_SUM)

void main()
{
    uint idx = addrGen(uvec2(uint(gl_FragCoord.x), uint(gl_FragCoord.y)));
#if defined(TEST_PIXEL_SYNC)
    // Code A - Pixel Sync
    beginInvocationInterlockARB();
    data[idx] += 1;
    endInvocationInterlockARB();
#elif defined(TEST_ATOMIC_OPERATIONS)
    // Code B - Atomic Operations
    atomicAdd(data[idx], 1);
#else
    // Code C - No Synchronization
    data[idx] += 1;
#endif
}

#endif
