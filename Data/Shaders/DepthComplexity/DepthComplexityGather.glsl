-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

out vec3 fragmentNormal;

// Model-view-projection matrix
uniform mat4 mvpMatrix;

void main()
{
	fragmentNormal = vertexNormal;
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#include "DepthComplexityHeader.glsl"

in vec3 fragmentNormal;

out vec4 fragColor;

// Color of the object
uniform vec4 color;
uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float specularExponent;
uniform float opacity;

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	// Area of mutual exclusion for fragments mapping to same pixel
	beginInvocationInterlockARB();
	atomicAdd(numFragmentsBuffer[addrGen(uvec2(x,y))], 1);
	endInvocationInterlockARB();

    // Just compute something so that normal and color uniform variables aren't optimized out
	fragColor = vec4(color + vec4(fragmentNormal, 1.0) + vec4(ambientColor, opacity) + vec4(diffuseColor, opacity)
	        + vec4(specularExponent*specularColor, opacity));
}
