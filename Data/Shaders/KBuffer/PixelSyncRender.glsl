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

#include "PixelSyncHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAdress.glsl"

out vec4 fragColor;

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	
	memoryBarrierBuffer();
	
	
	// Front-to-back blending
	// Color accumulator
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);

	// Iterate over all stored (transparent) fragments for this pixel
	uint index = MAX_NUM_NODES*pixelIndex;
	uint numFragments = numFragmentsBuffer[pixelIndex];
	for (uint i = 0; i < numFragments; i++)
	{
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(nodes[index].color);
		float alphaSrc = colorSrc.a;
		color.rgb = color.rgb + (1.0 - color.a) * alphaSrc * colorSrc.rgb;
		color.a = color.a + (1.0 - color.a) * alphaSrc;
		index++;
	}
	//color = vec4(color.rgb / (1.0 - color.a), 1.0 - color.a);
	color = vec4(color.rgb / color.a, color.a);
	
	
	// Back-to-front blending
	
	// Color accumulator
	/*vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	
	// Iterate over all stored (transparent) fragments for this pixel
	int offset = numFragmentsBuffer[pixelIndex]-1;
	int index = MAX_NUM_NODES*pixelIndex + offset;
	for (int i = offset; i >= 0; i--)
	{
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(nodes[index].color);
		float alphaSrc = colorSrc.a;
		float alphaOut = alphaSrc + color.a * (1.0 - alphaSrc);
		color.rgb = (alphaSrc * colorSrc.rgb + (1.0 - alphaSrc) * color.a * color.rgb) / alphaOut;
		//color.rgb = (alphaSrc * colorSrc.rgb + (1.0 - alphaSrc) * color.a * color.rgb);
		color.a = alphaOut;
		index--;
	}*/
	
	fragColor = color;//vec4(color.rgb, 1.0)
	//fragColor = nodes[MAX_NUM_NODES*pixelIndex].color;
}
