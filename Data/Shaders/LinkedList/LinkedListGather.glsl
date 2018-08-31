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

#include "LinkedListHeader.glsl"
#include "ColorPack.glsl"

in vec4 fragmentColor;
in vec3 fragmentNormal;
in vec3 fragmentPositonLocal;
//in vec3 fragmentPosView;

//out vec4 fragColor;

void main()
{
	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);
	int pixelIndex = viewportW*y + x;

	LinkedListFragmentNode frag;
	// Pseudo Phong shading
	vec4 bandColor = fragmentColor;
	float stripWidth = 2.0;
	if (mod(fragmentPositonLocal.x, 2.0*stripWidth) < stripWidth) {
		bandColor = vec4(1.0,1.0,1.0,1.0);
	}
	vec4 color = vec4(bandColor.rgb * (dot(fragmentNormal, vec3(1.0,0.0,0.0))/4.0+0.75), fragmentColor.a);
	frag.color = packColorRGBA(color);
	frag.depth = gl_FragCoord.z;
	frag.next = -1;
	
	//memoryBarrierBuffer();
	uint insertIndex = atomicCounterIncrement(fragCounter);
    if (insertIndex < linkedListSize) {
        frag.next = atomicExchange(startOffset[pixelIndex], insertIndex);
        fragmentBuffer[insertIndex] = frag;
    }
}
