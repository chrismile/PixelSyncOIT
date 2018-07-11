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

out vec4 fragColor;

void main()
{
	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);
	int pixelIndex = viewportW*y + x;
	
	memoryBarrierBuffer();
	
	
	// Front-to-back blending
	// Color accumulator
	/*vec4 color = vec4(0.0, 0.0, 0.0, 0.0);

	// Iterate over all stored (transparent) fragments for this pixel
	int index = nodesPerPixel*(viewportW*y + x);
	int numFragments = numFragmentsBuffer[pixelIndex];
	for (int i = 0; i < numFragments; i++)
	{
		// Blend the accumulated color with the color of the fragment node
		float alphaSrc = nodes[index].color.a;
		color.rgb = color.rgb + (1.0 - color.a) * alphaSrc * nodes[index].color.rgb;
		color.a = color.a + (1.0 - color.a) * alphaSrc;
		index++;
	}
	//color = vec4(color.rgb / (1.0 - color.a), 1.0 - color.a);
	color = vec4(color.rgb / color.a, color.a);*/
	
	
	// Back-to-front blending
	
	// Color accumulator
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	
	// Iterate over all stored (transparent) fragments for this pixel
	int offset = numFragmentsBuffer[pixelIndex]-1;
	int index = nodesPerPixel*(viewportW*y + x) + offset;
	for (int i = offset; i >= 0; i--)
	{
		// Blend the accumulated color with the color of the fragment node
		float alphaSrc = nodes[index].color.a;
		float alphaOut = alphaSrc + color.a * (1.0 - alphaSrc);
		color.rgb = (alphaSrc * nodes[index].color.rgb + (1.0 - alphaSrc) * color.a * color.rgb) / alphaOut;
		//color.rgb = (alphaSrc * nodes[index].color.rgb + (1.0 - alphaSrc) * color.a * color.rgb);
		color.a = alphaOut;
		index--;
	}
	
	fragColor = color;//vec4(color.rgb, 1.0)
	//fragColor = nodes[nodesPerPixel*(viewportW*y + x)].color;
}
