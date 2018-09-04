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

#include "PixelSyncHeader.glsl"
#include "ColorPack.glsl"
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

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	// Fragment index (in nodes buffer):
	uint index = MAX_NUM_NODES*pixelIndex;

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

	FragmentNode frag;
	frag.color = packColorRGBA(color);
	frag.depth = gl_FragCoord.z;

	// Area of mutual exclusion for fragments mapping to same pixel
	beginInvocationInterlockARB();

	memoryBarrierBuffer();

	// Use insertion sort to insert new fragment
	uint numFragments = numFragmentsBuffer[pixelIndex];
	for (uint i = 0; i < numFragments; i++)
	{
		if (frag.depth < nodes[index].depth)
		{
			FragmentNode temp = frag;
			frag = nodes[index];
			nodes[index] = temp;
		}
		index++;
	}
	
	// Store the fragment at the end of the list if capacity is left
	if (numFragments < MAX_NUM_NODES) {
		atomicAdd(numFragmentsBuffer[pixelIndex], 1);
		//numFragmentsBuffer[pixelIndex]++;
		nodes[index] = frag;
		//atomicCounterIncrement(numFragmentsBuffer[pixelIndex]);
		//numFragmentsBuffer[pixelIndex]++; // Race conditon on Intel if used here. Why?
	}

	memoryBarrierBuffer();
	
	// If no space was left to store the last fragment, simply discard it.
	// TODO: Merge nodes with least visual impact?
	/*if (frag.used == 1) {
		int lastIndex = index-1;
	
		// Blend with last fragment
		float alpha = nodes[lastIndex].color.a;
		float alphaOut = alpha + frag.color.a * (1.0 - alpha);
		nodes[lastIndex].color.rgb = (alpha * nodes[lastIndex].color.rgb + (1.0 - alpha) * frag.color.a * frag.color.rgb) / alphaOut;
		//color.rgb = (alpha * nodes[lastIndex].color.rgb + (1.0 - alpha) * frag.color.a * frag.color.rgb);
		nodes[lastIndex].color.a = alphaOut;
	}*/
		
	endInvocationInterlockARB();
	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
