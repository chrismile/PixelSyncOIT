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

#include "ColorPack.glsl"
#include "HTHeader.glsl"
#include "TiledAdress.glsl"

out vec4 fragColor;

void main()
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));

	memoryBarrierBuffer();

	// Read data from SSBO
	HTFragmentNode nodeArray[MAX_NUM_NODES];
	loadFragmentNodes(pixelIndex, nodeArray);

	HTFragmentTail tail;
    unpackFragmentTail(tails[pixelIndex], tail);


	// Read data from SSBO
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	float trans = 1.0;
	for (uint i = 0; i < MAX_NUM_NODES; i++) {
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(nodeArray[i].premulColor);
		//color.rgb = color.rgb + (1.0 - color.a) * colorSrc.rgb;
		//color.a = color.a + (1.0 - color.a) * alphaSrc;
		color.rgb = color.rgb + trans * colorSrc.rgb;
		trans *= colorSrc.a;
	}
	color.a = 1.0 - trans;

	// Blend with tail color
	//color = vec4(color.rgb / (1.0 - trans), 1.0 - trans);
	//color.rgb = color.rgb + trans * tailColor.rgb;

    //color.rgb = (color.rgb) / color.a;
    if (tail.accumFragCount > 0u) {
        float t = float(tail.accumFragCount);
        vec4 tailColor = vec4(tail.accumColor.rgb / tail.accumColor.a, 1.0 - pow(1.0 - tail.accumColor.a / t, t));
        //tailColor.rgb = tailColor.rgb * tailColor.a;

        color.rgb = color.rgb + (1.0 - color.a) * tailColor.rgb;
        color.a = color.a + (1.0 - color.a) * tailColor.a;
    }

	//fragColor = vec4(vec3(1.0-tailColor.r), 1.0);
	//fragColor = vec4(tailColor.rgb, 1.0);
	//fragColor = vec4(color.rgb / color.a, color.a);

    //float lum = 1.0 - pow(1.0 - tail.accumColor.a / t, t);

    // Make sure data is cleared for next rendering pass
    clearPixel(pixelIndex);

	//fragColor = tailColor;
	fragColor = vec4(color.rgb / color.a, color.a);
	//float lum = tailColor.a;
	//fragColor = vec4(tailColor.rgb, 1.0);

	/*if (tail.accumFragCount > 0u) {
        float t = float(tail.accumFragCount);
	    vec4 tailColor = vec4(tail.accumColor.rgb / tail.accumColor.a, 1.0 - pow(1.0 - tail.accumColor.a / t, t));
	    fragColor = vec4(tailColor.rgba);
	}*/

}
