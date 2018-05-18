-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

out vec4 fragmentColor;
out vec3 fragmentNormal;
out vec3 fragmentPositonLocal;

// Model-view-projection matrix
uniform mat4 mvpMatrix;
uniform mat4 mMatrix;
uniform mat4 vMatrix;

// Color of the object
uniform vec4 color;

void main()
{
	fragmentColor = color;
	fragmentNormal = vertexNormal;
	fragmentPositonLocal = (vec4(vertexPosition, 1.0)).xyz;
	//fragmentPosView = (vMatrix * mMatrix * vec4(vertexPosition, 1.0)).xyz;
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_image_load_store.txt
#extension GL_ARB_shader_image_load_store : require

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_fragment_shader_interlock.txt
#extension GL_ARB_fragment_shader_interlock : require

// Use early z-test to cull transparent fragments occluded by opaque fragments.
// Additionaly, use fragment interlock.
layout(early_fragment_tests, pixel_interlock_unordered) in;
// NOTE to myself: early_fragment_tests is evil :(

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

in vec4 fragmentColor;
in vec3 fragmentNormal;
in vec3 fragmentPositonLocal;
//in vec3 fragmentPosView;

//out vec4 fragColor;

// Number of transparent pixels we can store per node
uniform int nodesPerPixel;

uniform int viewportW;
//uniform int viewportH; // Not needed

void main()
{
	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);
	int index = nodesPerPixel*(viewportW*y + x);
	//int index = nodesPerPixel*int(viewportW*gl_FragCoord.y + gl_FragCoord.x);

	FragmentNode frag;
	// Pseudo Phong shading
	vec4 bandColor = fragmentColor;
	float stripWidth = 2.0;
	if (mod(fragmentPositonLocal.x, 2.0*stripWidth) < stripWidth) {
		bandColor = vec4(1.0,1.0,1.0,1.0);
	}
	frag.color = vec4(bandColor.rgb * (dot(fragmentNormal, vec3(1.0,0.0,0.0))/4.0+0.75), fragmentColor.a);
	//frag.depth = fragmentPosView.z;
	frag.depth = gl_FragCoord.z;
	frag.used = 1;
	
	// Area of mutual exclusion for fragments mapping to same pixel
	beginInvocationInterlockARB();
	
	memoryBarrierBuffer();
	
	// Use bubble sort to insert new fragment
	for (int i = 0; i < nodesPerPixel; i++)
	{
		if (nodes[index].used == 0)
		{
			nodes[index] = frag;
			frag.used = 0;
			break;
		}
		else if (frag.depth < nodes[index].depth)
		{
			FragmentNode temp = frag;
			frag = nodes[index];
			nodes[index] = temp;
		}
		index++;
	}
	
	// If no space was left to store the last fragment, simply discard it.
	// TODO: Merge nodes with least visual impact.
	if (frag.used == 1) {
		int lastIndex = index-1;
	
		// Blend with last fragment
		float alpha = nodes[lastIndex].color.a;
		float alphaOut = alpha + frag.color.a * (1.0 - alpha);
		nodes[lastIndex].color.rgb = (alpha * nodes[lastIndex].color.rgb + (1.0 - alpha) * frag.color.a * frag.color.rgb) / alphaOut;
		//color.rgb = (alpha * nodes[lastIndex].color.rgb + (1.0 - alpha) * frag.color.a * frag.color.rgb);
		nodes[lastIndex].color.a = alphaOut;
	}
		
	endInvocationInterlockARB();
	//fragColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
}
