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
#include "HTHeader.glsl"
#include "TiledAdress.glsl"

in vec4 fragmentColor;
in vec3 fragmentNormal;
in vec3 fragmentPositonLocal;
//in vec3 fragmentPosView;

//out vec4 fragColor;

// Adapted version of Hybrid Transparency [Maule et al. 2014]
void hybridTransparencyBlending(in HTFragmentNode frag, inout HTFragmentNode list[MAX_NUM_NODES],
        inout HTFragmentTail tail, inout uint numFragments)
{
	HTFragmentNode temp;
	for (int i = 0; i < int(numFragments)-1; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    if (numFragments == MAX_NUM_NODES-1u) {
    	// Update tail (accumulates result)
        tail.accumColor.rgb += frag.premulColor.rgb;
        tail.accumAlpha += frag.premulColor.a;
        tail.accumFragCount++;
    } else {
        list[numFragments-1u] = merge;
        numFragments++;
    }
}


void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	HTFragmentNode_compressed frag;
	// Pseudo Phong shading
	vec4 bandColor = fragmentColor;
	float stripWidth = 2.0;
	if (mod(fragmentPositonLocal.x, 2.0*stripWidth) < stripWidth) {
		bandColor = vec4(1.0,1.0,1.0,1.0);
	}
	vec4 color = vec4(bandColor.rgb * (dot(fragmentNormal, vec3(1.0,0.0,0.0))/4.0+0.75), fragmentColor.a);
	frag.premulColor = packColorRGBA(vec4(color.rgb * color.a, color.a));
	frag.depth = gl_FragCoord.z;


    // Begin of actual algorithm code
	HTFragmentNode localFragmentList[MAX_NUM_NODES];
	HTFragmentTail localTailFragment;

	beginInvocationInterlockARB();

    uint numFragments = numFragmentsBuffer[pixelIndex];

	// Read data from SSBO
	for (int i = 0; i < numFragments; i++) {
		 unpackFragmentNode(nodes[pixelIndex].nodes[i], localFragmentList[i]);
	}
	unpackFragmentTail(nodes[pixelIndex].tail, localTailFragment);

	// Insert node to list (or merge last two nodes)
	hybridTransparencyBlending(frag, localFragmentList, localTailFragment, numFragments);
	numFragmentsBuffer[pixelIndex] = numFragments;

	// Write back data to SSBO
	for (int i = 0; i < numFragments; i++) {
		packFragmentNode(localFragmentList[i], nodes[i+offset]);
	}
	packFragmentTail(localTailFragment, nodes[pixelIndex].tail);

	endInvocationInterlockARB();
}

