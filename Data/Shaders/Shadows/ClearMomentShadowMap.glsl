-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
    gl_Position = vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
#extension GL_ARB_shader_image_load_store : require

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer) in vec4 gl_FragCoord;


uniform int viewportW;

layout (binding = 3, r32f) coherent uniform image2DArray zeroth_moment_shadow; // float
#if SINGLE_PRECISION_SHADOW
#if NUM_MOMENTS_SHADOW == 6
layout (binding = 4, rg32f) coherent uniform image2DArray moments_shadow; // vec2
#if USE_R_RG_RGBA_FOR_MBOIT6_SHADOW
layout (binding = 5, rgba32f) coherent uniform image2DArray extra_moments_shadow; // vec4
#endif
#else
layout (binding = 4, rgba32f) coherent uniform image2DArray moments_shadow; // vec4
#endif
#else
#if NUM_MOMENTS_SHADOW == 6
layout (binding = 4, rg16) coherent uniform image2DArray moments_shadow;
#if USE_R_RG_RGBA_FOR_MBOIT6_SHADOW
layout (binding = 5, rgba16) coherent uniform image2DArray extra_moments_shadow;
#endif
#else
layout (binding = 4, rgba16) coherent uniform image2DArray moments_shadow;
#endif
#endif

void clearMoments(ivec3 idx0)
{
    ivec3 idx1 = ivec3(idx0.xy, 1);
    ivec3 idx2 = ivec3(idx0.xy, 2);

    imageStore(zeroth_moment_shadow, idx0, vec4(0.0));
    imageStore(moments_shadow, idx0, vec4(0.0));
#if NUM_MOMENTS_SHADOW == 6
#if USE_R_RG_RGBA_FOR_MBOIT6_SHADOW
    imageStore(extra_moments_shadow, idx0, vec4(0.0));
#else
    imageStore(moments_shadow, idx1, vec4(0.0));
    imageStore(moments_shadow, idx2, vec4(0.0));
#endif
#elif NUM_MOMENTS_SHADOW == 8
    imageStore(moments_shadow, idx1, vec4(0.0));
#endif
}

void main()
{
    ivec3 idx0 = ivec3(ivec2(gl_FragCoord.xy), 0);

    // Make sure data is cleared for next rendering pass
    clearMoments(idx0);
}

