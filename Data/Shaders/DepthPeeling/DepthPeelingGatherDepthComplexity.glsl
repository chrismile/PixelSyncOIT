-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

out vec3 screenSpacePosition;

// Color of the object
uniform vec4 color;

void main()
{
	screenSpacePosition = (vMatrix * mMatrix * vec4(vertexPosition, 1.0)).xyz;
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

in vec3 screenSpacePosition;

#include "DepthComplexityGather.glsl"


void main()
{
#ifdef REQUIRE_INVOCATION_INTERLOCK
	// Area of mutual exclusion for fragments mapping to the same pixel
	beginInvocationInterlockARB();
	gatherFragment(vec4(0.0));
	endInvocationInterlockARB();
#else
	gatherFragment(vec4(0.0));
#endif
}
