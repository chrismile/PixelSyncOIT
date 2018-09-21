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

	// Color accumulator
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);

    // Collect all fragments for this pixel
	int numFrags = 0;
	LinkedListFragmentNode fragment;
	// Insert using 1-pass bubble sort
	/*for (int i = 0; i < MAX_NUM_FRAGS; i++)
	{
        if (fragOffset == -1) {
            // End of list reached
            break;
        }

        fragment = fragmentBuffer[fragOffset];
		fragOffset = fragment.next;

        // Insert using 1-pass bubble sort
        for (int index = 0; index < numFrags; index++)
        {
            if (fragment.depth < fragList[index].depth)
            {
                LinkedListFragmentNode temp = fragment;
                fragment = fragList[index];
                fragList[index] = temp;
            }
        }
        fragList[numFrags] = fragment;

		numFrags++;
	}

    // Blend all fragments in the right order
	for (int i = 0; i < numFrags; i++) {
		fragment = fragList[i];

        // Back-to-Front (BTF)
		// Blend the accumulated color with the color of the fragment node
		//float alpha = fragment.color.a;
		//float alphaOut = alpha + color.a * (1.0 - alpha);
		//color.rgb = (alpha * fragment.color.rgb + (1.0 - alpha) * color.a * color.rgb) / alphaOut;
		//color.a = alphaOut;

		// Front-to-Back (FTB)
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(fragment.color);
		float alphaSrc = colorSrc.a;
		color.rgb = color.rgb + (1.0 - color.a) * alphaSrc * colorSrc.rgb;
		color.a = color.a + (1.0 - color.a) * alphaSrc;
	}
	color = vec4(color.rgb / color.a, color.a);*/

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

	color = sortingAlgorithm(numFrags);

    // TODO: Stencil
	fragColor = color;
}