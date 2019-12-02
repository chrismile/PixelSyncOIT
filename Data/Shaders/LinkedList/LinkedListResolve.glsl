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

    LinkedListFragmentNode fragment;

    vec4 intermediateColor = vec4(0);
    bool isFinished = false;
    uint totalNumFrags = 0;

    while (!isFinished)
    {
        uint numFrags = 0;

        for (uint i = 0; i < MAX_NUM_FRAGS; i++)
        {
            if (fragOffset == -1) {
                // End of list reached
                isFinished = true;
                break;
            }

            fragment = fragmentBuffer[fragOffset];
            fragOffset = fragment.next;

            colorList[i] = fragment.color;
            depthList[i] = fragment.depth;

            numFrags++;
        }

        totalNumFrags += numFrags;

        if (numFrags == 0) {
            isFinished = true;
            break;
        }
        else
        {
            vec4 curColor = sortingAlgorithm(numFrags);
            // blend colors front-to-back
            vec4 colorSrc = curColor;
            float alphaSrc = colorSrc.a;
            intermediateColor.rgb = intermediateColor.rgb + (1.0 - intermediateColor.a) * alphaSrc * colorSrc.rgb;
            intermediateColor.a = intermediateColor.a + (1.0 - intermediateColor.a) * alphaSrc;

            if (intermediateColor.a > 0.99)
            {
                isFinished = true;
                break;
            }
        }
    }

    if (totalNumFrags == 0)
    {
        discard;
    }

    fragColor = vec4(intermediateColor.rgb / intermediateColor.a, intermediateColor.a);
//    fragColor = vec4(0,0,float(totalNumFrags) / 45.0f,1);
}