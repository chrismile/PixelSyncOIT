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

out vec4 fragColor;

uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float specularExponent;
uniform float opacity;
uniform int bandedColorShading = 1;

// Adapted version of Hybrid Transparency [Maule et al. 2014]
void hybridTransparencyBlending(in HTFragmentNode frag, inout HTFragmentNode list[MAX_NUM_NODES],
        inout HTFragmentTail tail)
{
	HTFragmentNode temp;
	// Use bubble sort to insert new fragment node (single pass)
	for (int i = 0; i < MAX_NUM_NODES; i++) {
		if (frag.depth <= list[i].depth) {
			temp = list[i];
			list[i] = frag;
			frag = temp;
		}
	}

    if (frag.depth != DISTANCE_INFINITE) {
    	// Update tail (accumulates result)
    	vec4 fragColor = unpackColorRGBA(frag.premulColor);
        tail.accumColor.rgb += fragColor.rgb;
        tail.accumColor.a += 1.0 - fragColor.a;
        tail.accumFragCount += 1u;
    }
}


void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	// Pseudo Phong shading
	vec4 bandColor = fragmentColor;
	float stripWidth = 2.0;
	if (mod(fragmentPositonLocal.x, 2.0*stripWidth) < stripWidth) {
		bandColor = vec4(1.0,1.0,1.0,1.0);
	}
	vec4 color = vec4(bandColor.rgb * (dot(fragmentNormal, vec3(1.0,0.0,0.0))/4.0+0.75), fragmentColor.a);

	if (bandedColorShading == 0) {
	    vec3 lightDirection = vec3(1.0,0.0,0.0);
	    vec3 ambientShading = ambientColor * 0.1;
	    vec3 diffuseShading = diffuseColor * clamp(dot(fragmentNormal, lightDirection)/2.0+0.75, 0.0, 1.0);
	    vec3 specularShading = specularColor * specularExponent * 0.00001; // In order to not get an unused warning
	    color = vec4(ambientShading + diffuseShading + specularShading, opacity * fragmentColor.a);
	}


	HTFragmentNode frag;
	frag.depth = gl_FragCoord.z;
	frag.premulColor = packColorRGBA(vec4(color.rgb * color.a, 1.0 - color.a));


    // Begin of actual algorithm code
	HTFragmentNode nodeArray[MAX_NUM_NODES];
	HTFragmentTail tail;

	beginInvocationInterlockARB();

	// Read data from SSBO
	loadFragmentNodes(pixelIndex, nodeArray);
	unpackFragmentTail(tails[pixelIndex], tail);

	// Insert node to list
	hybridTransparencyBlending(frag, nodeArray, tail);

	// Write back data to SSBO
	storeFragmentNodes(pixelIndex, nodeArray);
	packFragmentTail(tail, tails[pixelIndex]);

	endInvocationInterlockARB();

	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
