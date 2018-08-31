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

#include "ColorPack.glsl"
#include "MLABHeader.glsl"
#include "TiledAdress.glsl"

in vec4 fragmentColor;
in vec3 fragmentNormal;
in vec3 fragmentPositonLocal;

// Adapted version of "Multi-Layer Alpha Blending" [Salvi and Vaidyanathan 2014]
void multiLayerAlphaBlending(in MLABFragmentNode frag, inout MLABFragmentNode list[MAX_NUM_NODES+1])
{
	MLABFragmentNode temp, merge;
	// Use bubble sort to insert new fragment node (single pass)
	for (int i = 0; i < MAX_NUM_NODES+1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    // Merge last two nodes if necessary
    if (list[MAX_NUM_NODES].depth != DISTANCE_INFINITE) {
        vec4 src = unpackColorRGBA(list[MAX_NUM_NODES-1].premulColor);
        vec4 dst = unpackColorRGBA(list[MAX_NUM_NODES].premulColor);
        vec4 mergedColor;
        mergedColor.rgb = src.rgb + dst.rgb * src.a;
        mergedColor.a = src.a * dst.a; // Transmittance
        merge.premulColor = packColorRGBA(mergedColor);
        merge.depth = list[MAX_NUM_NODES-1].depth;
        list[MAX_NUM_NODES-1] = merge;
	}
}


void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	uint offset = MAX_NUM_NODES*pixelIndex;

	// Pseudo Phong shading
	vec4 bandColor = fragmentColor;
	float stripWidth = 2.0;
	if (mod(fragmentPositonLocal.x, 2.0*stripWidth) < stripWidth) {
		bandColor = vec4(1.0,1.0,1.0,1.0);
	}
	vec4 color = vec4(bandColor.rgb * (dot(fragmentNormal, vec3(1.0,0.0,0.0))/4.0+0.75), fragmentColor.a);

	MLABFragmentNode frag;
	frag.depth = gl_FragCoord.z;
	frag.premulColor = packColorRGBA(vec4(color.rgb * color.a, 1.0 - color.a));


    // Begin of actual algorithm code
	MLABFragmentNode nodeArray[MAX_NUM_NODES+1];

	beginInvocationInterlockARB();

	// Read data from SSBO
	loadFragmentNodes(pixelIndex, nodeArray);

	// Insert node to list (or merge last two nodes)
	multiLayerAlphaBlending(frag, nodeArray);

	// Write back data to SSBO
	storeFragmentNodes(pixelIndex, nodeArray);

	endInvocationInterlockARB();
}

