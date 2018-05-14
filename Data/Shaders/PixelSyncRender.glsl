-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
	fragmentColor = vertexColor;
	gl_Position = mvpMatrix * vertexPosition;
}


-- Fragment

#version 430 core

#extension ARB_shader_image_load_store : require

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
	//uint padding1;
	//uint padding2;
};

// Stores renderTargetWidth * renderTargetHeight * nodesPerPixel fragments.
// Access fragment i at screen position (x,y) using "nodes[w*npp*y + npp*x + i]".
layout (std430, binding = 0) buffer FragmentNodes
{
	FragmentNode nodes[];
};

void main()
{
	// Color accumulator
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	
	// Iterate over all stored (transparent) fragments for this pixel
	for (int i = 0; i < nodesPerPixel; i++)
	{
		if (nodes[index].used == 0)
		{
			break;
		}
		
		// Blend the accumulated color with the color of the fragment node
		color = OP1 * color + OP2 * nodes[index].color;
		index++;
	}
	
	gl_FragColor = color;
}