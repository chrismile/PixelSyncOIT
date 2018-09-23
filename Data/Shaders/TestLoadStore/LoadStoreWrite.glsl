-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer) in vec4 gl_FragCoord;

layout (binding = 0, r32f) uniform image2DArray moments; // float

out vec4 fragColor;

void main()
{
	ivec3 idx0 = ivec3(ivec2(gl_FragCoord.xy), 0);
	ivec3 idx1 = ivec3(ivec2(gl_FragCoord.xy), 1);
    imageStore(moments, idx0, vec4(0.8));
    imageStore(moments, idx1, vec4(1.0));
    fragColor = vec4(0.0);
}