-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

// Model-view-projection matrix
uniform mat4 mvpMatrix;

void main()
{
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#include "ColorPack.glsl"
#include "HTHeader.glsl"
#include "TiledAdress.glsl"

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	numFragmentsBuffer[pixelIndex] = 0;

    // TODO: Clear mask?
	HTFragmentTail_compressed tail;
	tail.accumColor = 0u;
	tail.accumAlphaAndCount = 0u;
	nodes[pixelIndex].tail = tail;
}
