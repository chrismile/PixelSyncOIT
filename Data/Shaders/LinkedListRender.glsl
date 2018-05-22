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

#include "LinkedListHeader.glsl"

out vec4 fragColor;

void main()
{
	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);
	int pixelIndex = viewportW*y + x;
	
	// Get start offset from array
	uint fragOffset = startOffset[pixelIndex];
	
	// Color accumulator
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	
	// Front-to-back blending
	// Iterate over all stored (transparent) fragments for this pixel
	/*while (fragOffset != -1)
	{
		LinkedListFragmentNode fragment = fragmentBuffer[fragOffset];

		// Blend the accumulated color with the color of the fragment node
		float alpha = fragment.color.a;
		color.rgb = color.rgb + (1.0 - color.a) * alpha * fragment.color.rgb;
		//color.rgb = vec3(1.0, 0.0, 1.0);
		color.a = color.a + (1.0 - color.a) * alpha;
		
		fragOffset = fragment.next;
	}*/
	// Back-to-front blending
	// Iterate over all stored (transparent) fragments for this pixel
	while (fragOffset != -1)
	{
		LinkedListFragmentNode fragment = fragmentBuffer[fragOffset];
		
		// Blend the accumulated color with the color of the fragment node
		float alpha = fragment.color.a;
		float alphaOut = alpha + color.a * (1.0 - alpha);
		color.rgb = (alpha * fragment.color.rgb + (1.0 - alpha) * color.a * color.rgb) / alphaOut;
		//color.rgb = (alpha * fragment.color.rgb + (1.0 - alpha) * color.a * color.rgb);
		color.a = alphaOut;
		
		fragOffset = fragment.next;
	}
	
	fragColor = color;//vec4(color.rgb, 1.0);
	//fragColor = nodes[nodesPerPixel*(viewportW*y + x)].color;
}