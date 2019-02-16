-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#include "ColorPack.glsl"
#include "HTHeader.glsl"
#include "TiledAddress.glsl"

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
        color.rgb = color.rgb + trans * colorSrc.rgb;
        trans *= colorSrc.a;
    }
    color.a = 1.0 - trans;

    if (tail.accumFragCount > 0u && color.a < 0.999) {
        float t = float(tail.accumFragCount);
        vec4 tailColor = vec4(tail.accumColor.rgb / tail.accumColor.a, 1.0 - pow(1.0 - tail.accumColor.a / t, t));

        color.rgb = color.rgb + (1.0 - color.a) * tailColor.rgb;
        color.a = color.a + (1.0 - color.a) * tailColor.a;
    }

    // Make sure data is cleared for next rendering pass
    clearPixel(pixelIndex);

    fragColor = vec4(color.rgb / color.a, color.a);
}
