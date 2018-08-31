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
//in vec3 fragmentPosView;

//out vec4 fragColor;

// Adapted version of Multi-Layer Alpha Blending [Salvi and Vaidyanathan 2014]
void multiLayerAlphaBlending(in MLABFragmentNode frag, inout MLABFragmentNode list[MAX_NUM_NODES],
        inout uint numFragments)
{
	MLABFragmentNode temp;
	for (int i = 0; i < int(numFragments)-1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

	MLABFragmentNode merge;
    if (numFragments == MAX_NUM_NODES-1u) {
    	// Merge last two nodes
        merge.premulColor.rgb = list[MAX_NUM_NODES-1].premulColor.rgb
                + frag.premulColor.rgb * list[MAX_NUM_NODES-1].premulColor.a;
        merge.premulColor.a = list[MAX_NUM_NODES-1].premulColor.a * frag.premulColor.a;
        merge.depth = list[MAX_NUM_NODES-1].depth;
    } else {
        merge = frag;
        numFragments++;
    }
	list[numFragments-1u] = merge;
}


void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	uint offset = MAX_NUM_NODES*pixelIndex;

	MLABFragmentNode frag;
	// Pseudo Phong shading
	vec4 bandColor = fragmentColor;
	float stripWidth = 2.0;
	if (mod(fragmentPositonLocal.x, 2.0*stripWidth) < stripWidth) {
		bandColor = vec4(1.0,1.0,1.0,1.0);
	}
	vec4 color = vec4(bandColor.rgb * (dot(fragmentNormal, vec3(1.0,0.0,0.0))/4.0+0.75), fragmentColor.a);
	frag.premulColor = vec4(color.rgb * color.a, color.a);
	frag.depth = gl_FragCoord.z;


    // Begin of actual algorithm code
	MLABFragmentNode localFragmentList[MAX_NUM_NODES];

	beginInvocationInterlockARB();

    uint numFragments = numFragmentsBuffer[pixelIndex];

	// Read data from SSBO
	for (int i = 0; i < numFragments; i++) {
		 unpackFragmentNode(nodes[i+offset], localFragmentList[i]);
	}

	// Insert node to list (or merge last two nodes)
	multiLayerAlphaBlending(frag, localFragmentList, numFragments);
	numFragmentsBuffer[pixelIndex] = numFragments;

	// Write back data to SSBO
	for (int i = 0; i < numFragments; i++) {
		packFragmentNode(localFragmentList[i], nodes[i+offset]);
	}

	endInvocationInterlockARB();
}

