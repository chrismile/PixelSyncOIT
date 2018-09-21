-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
#extension GL_ARB_shader_image_load_store : require

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer)
in vec4 gl_FragCoord;


out vec4 fragColor;

layout (binding = 0, r32f) uniform image2DArray zeroth_moment; // float
layout (binding = 1, rgba32f) uniform image2DArray moments; // vec4

uniform sampler2D transparentSurfaceAccumulator;

void main()
{
    ivec3 idx0 = ivec3(ivec2(gl_FragCoord.xy), 0);
    vec4 color = texelFetch(transparentSurfaceAccumulator, idx0.xy, 0);
    float b_0 = imageLoad(zeroth_moment, idx0).x;
    if (b_0 < 0.00100050033f) {
        discard;
    }
    float total_transmittance = exp(-b_0);
    if (isinf(b_0)) {
        total_transmittance = 0.0f;
    }

    // Make sure data is cleared for next rendering pass
    imageStore(zeroth_moment, idx0, vec4(0.0));
    imageStore(moments, idx0, vec4(0.0));


    //color_blend = exp(-b_0) * L_n + (1 - exp(-b_0)) * weighted_color
    fragColor = vec4(color.rgb / color.a, 1.0 - total_transmittance);
    //fragColor = vec4(color.rgb, 1.0);
}

