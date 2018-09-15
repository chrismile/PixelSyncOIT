-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

// Model-view-projection matrix
uniform mat4 mvpMatrix;

void main()
{
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core
#define MOMENT_GENERATION 0

#include "MBOITHeader.glsl"
#include "MomentOIT.glsl"

out vec4 fragColor;

void main()
{
	float depth = gl_FragCoord.z; // TODO: Linear [-1,1] ?
	float transmittance_at_depth, total_transmittance;
	resolveMoments(transmittance_at_depth, total_transmittance, depth, gl_FragCoord.xy);
	color = vec4(color.rgb, transmittance_at_depth * color.a);

    // Make sure data is cleared for next rendering pass
    clearPixel(pixelIndex);

    float alphaOut = 1.0 - trans;
	fragColor = vec4(color.rgb / alphaOut, alphaOut);
}
