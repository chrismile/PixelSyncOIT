-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

out vec4 fragmentColor;
out vec3 fragmentNormal;
out vec3 fragmentPositonLocal;

// Color of the object
uniform vec4 color;

void main()
{
	fragmentColor = color;
	fragmentNormal = vertexNormal;
	fragmentPositonLocal = (vec4(vertexPosition, 1.0)).xyz;
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#ifndef DIRECT_BLIT_GATHER
#include OIT_GATHER_HEADER
#endif

in vec4 fragmentColor;
in vec3 fragmentNormal;
in vec3 fragmentPositonLocal;

#ifdef DIRECT_BLIT_GATHER
out vec4 fragColor;
#endif

uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float specularExponent;
uniform float opacity;
uniform int bandedColorShading = 1;

void main()
{
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
	    vec3 specularShading = specularColor * specularExponent * 0.00001; // In order not to get an unused warning
	    color = vec4(ambientShading + diffuseShading + specularShading, opacity * fragmentColor.a);
	}

#ifdef DIRECT_BLIT_GATHER
	// Direct rendering
	fragColor = color;
#else
#ifdef REQUIRE_INVOCATION_INTERLOCK
	// Area of mutual exclusion for fragments mapping to the same pixel
	beginInvocationInterlockARB();
	gatherFragment(color);
	endInvocationInterlockARB();
#else
	gatherFragment(color);
#endif
#endif
}
