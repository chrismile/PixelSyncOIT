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

#include "ATHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAdress.glsl"

out vec4 fragColor;

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	memoryBarrierBuffer();

	// Read data from SSBO
	ATFragmentNode nodeArray[MAX_NUM_NODES+1];
	loadFragmentNodes(pixelIndex, nodeArray);

	// Read data from SSBO
	vec3 color = vec3(0.0, 0.0, 0.0);
	float trans = 1.0;
	for (uint i = 0; i < MAX_NUM_NODES; i++) {
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(nodeArray[i].premulColor);
		//color.rgb = color.rgb + (1.0 - color.a) * colorSrc.rgb;
		//color.a = color.a + (1.0 - color.a) * alphaSrc;
		trans = colorSrc.a;
		color.rgb = color.rgb + trans * colorSrc.rgb;
	}

    // Make sure data is cleared for next rendering pass
    clearPixel(pixelIndex);

    float alphaOut = 1.0 - trans;
	fragColor = vec4(color.rgb / alphaOut, alphaOut);
}
