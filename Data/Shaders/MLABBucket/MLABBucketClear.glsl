-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#include "MLABBucketHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAddress.glsl"

void main()
{
    ivec2 fragPos2D = ivec2(int(gl_FragCoord.x), int(gl_FragCoord.y));
    clearPixel(addrGen(uvec2(fragPos2D)), fragPos2D);
}
