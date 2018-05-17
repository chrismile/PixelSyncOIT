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

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
#extension GL_ARB_shader_image_load_store : require

// gl_FragCoord will be used for pixel centers at integer coordinates.
// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
layout(pixel_center_integer) in vec4 gl_FragCoord;

// A fragment node stores rendering information about one specific fragment
struct FragmentNode
{
	// RGBA color of the node
	vec4 color;
	// Depth value of the fragment (in view space)
	float depth;
	// Whether the node is empty or used
	uint used;
	
	// Padding to 2*vec4
	uint padding1;
	uint padding2;
};

// Stores viewportW * viewportH * nodesPerPixel fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) buffer FragmentNodes
{
	FragmentNode nodes[];
};

out vec4 fragColor;

// Number of transparent pixels we can store per node
uniform int nodesPerPixel;

uniform int viewportW;
//uniform int viewportH; // Not needed

void main()
{
	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);
	int index = nodesPerPixel*(viewportW*y + x) + nodesPerPixel-1;
	
	// Color accumulator
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	//vec4 color = vec4(1.0, 1.0, 1.0, 1.0);
	
	memoryBarrierBuffer();
	// Front-to-back blending
	// Iterate over all stored (transparent) fragments for this pixel
	for (int i = 0; i < nodesPerPixel; i++)
	{
		if (nodes[index].used == 0)
		{
			index++;
			continue;
		}

		// Blend the accumulated color with the color of the fragment node
		float alpha = nodes[index].color.a;
		color.rgb = color.rgb + (1.0 - color.a) * alpha * nodes[index].color.rgb;
		//color.rgb = vec3(1.0, 0.0, 1.0);
		color.a = color.a + (1.0 - color.a) * alpha;
		index++;
	}
	// Back-to-front blending
	// Iterate over all stored (transparent) fragments for this pixel
	/*for (int i = nodesPerPixel-1; i >= 0; i--)
	{
		//color = nodes[index].color;
		//if (mod(i, 4) == 0)
			//nodes[index].color = vec4(mod(float(index)/float(viewportW*nodesPerPixel), 1.0),0.0,0.0,1.0);
	
		if (nodes[index].used == 0)
		{
			index--;
			continue;
		}
		
		// Blend the accumulated color with the color of the fragment node
		float alpha = nodes[index].color.a;
		float alphaOut = alpha + color.a * (1.0 - alpha);
		color.rgb = (alpha * nodes[index].color.rgb + (1.0 - alpha) * color.a * color.rgb) / alphaOut;
		//color.rgb = (alpha * nodes[index].color.rgb + (1.0 - alpha) * color.a * color.rgb);
		color.a = alphaOut;
		index--;
	}*/
	
	fragColor = vec4(color.rgb, 1.0);
}