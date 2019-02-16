-- Vertex

#version 430 core

layout(location = 0) in vec3 vertexPosition;

void main()
{
    gl_Position = mvpMatrix * vec4(vertexPosition, 1.0);
}


-- Fragment

#version 430 core

#include "LinkedListHeader.glsl"
#include "ColorPack.glsl"

//#define MAX_NUM_FRAGS 256
uint colorList[MAX_NUM_FRAGS];
float depthList[MAX_NUM_FRAGS];


#include "LinkedListSort.glsl"

out vec4 fragColor;

void main()
{
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    int pixelIndex = viewportW*y + x;

    // Get start offset from array
    uint fragOffset = startOffset[pixelIndex];

    // Collect all fragments for this pixel
    int numFrags = 0;
    LinkedListFragmentNode fragment;
    for (int i = 0; i < MAX_NUM_FRAGS; i++)
    {
        if (fragOffset == -1) {
            // End of list reached
            break;
        }

        fragment = fragmentBuffer[fragOffset];
        fragOffset = fragment.next;

        colorList[i] = fragment.color;
        depthList[i] = fragment.depth;

        numFrags++;
    }

    if (numFrags == 0) {
        discard;
    }

    fragColor = sortingAlgorithm(numFrags);
}