-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
	gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#include "MLABBucketHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAddress.glsl"

out vec4 fragColor;

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	memoryBarrierBuffer();

	// Read data from SSBO
	MLABBucketFragmentNode nodeArray[BUFFER_SIZE+1];
	loadFragmentNodes(pixelIndex, nodeArray);

#ifdef MLAB_OPACITY_BUCKETS
	// Sort the nodes if the buckets are not already sorted by depth
    MLABBucketFragmentNode tmp;
	bool changed;
    do {
        changed = false;
        for (uint i = 0; i < BUFFER_SIZE - 1; ++i) {
            if(nodeArray[i].depth > nodeArray[i+1].depth) {
                tmp = nodeArray[i];
                nodeArray[i] = nodeArray[i+1];
                nodeArray[i+1] = tmp;
                changed = true;
            }
        }
    } while (changed);
#endif

	// Read data from SSBO
	vec3 color = vec3(0.0, 0.0, 0.0);
	float trans = 1.0;
	for (uint i = 0; i < BUFFER_SIZE; i++) {
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackUnorm4x8(nodeArray[i].premulColor);
		color.rgb = color.rgb + trans * colorSrc.rgb;
		trans *= colorSrc.a;
	}

    // Make sure data is cleared for next rendering pass
    clearPixel(pixelIndex);

    float alphaOut = 1.0 - trans;
    fragColor = vec4(color.rgb / alphaOut, alphaOut);
    //fragColor = vec4(vec3(alphaOut), 1.0); // Output opacity
}
