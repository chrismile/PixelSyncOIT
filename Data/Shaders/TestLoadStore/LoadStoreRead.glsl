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

out vec4 fragColor;

layout (binding = 0, r32f) uniform image2DArray moments; // float

void main()
{
	ivec3 idx0 = ivec3(ivec2(gl_FragCoord.xy), 0);
	ivec3 idx1 = ivec3(ivec2(gl_FragCoord.xy), 1);
	vec4 color0 = imageLoad(moments, idx0);
	vec4 color1 = imageLoad(moments, idx1);
	fragColor = vec4(color0.r, color1.r, 0.0, 1.0);
}