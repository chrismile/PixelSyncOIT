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

out vec4 fragColor;

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	uint offset = MAX_NUM_NODES*pixelIndex;

	memoryBarrierBuffer();

	// Read data from SSBO
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	HTFragmentTail tail;
	unpackFragmentTail(nodes[pixelIndex].tail, tail);
	float t = float(tail.accumFragCount);
	vec4 tailColor = vec4(tail.accumColor / tail.accumColor.a, 1.0 - pow(1.0 - tail.accumColor.a / t, t));
	uint numFragments = numFragmentsBuffer[pixelIndex];
	for (uint i = 0; i < numFragments; i++) {
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(nodes[i+offset].premulColor);
		float alphaSrc = colorSrc.a;
		color.rgb = color.rgb + (1.0 - color.a) * colorSrc.rgb;
		color.a = color.a + (1.0 - color.a) * alphaSrc;
	}

	color.a = color.a + (1 - color.a) * tailColor.a;
	color.rgb = (color.rgb + (1 - color.a) * tailColor.rgb) / color.a;

	fragColor = color;
}
