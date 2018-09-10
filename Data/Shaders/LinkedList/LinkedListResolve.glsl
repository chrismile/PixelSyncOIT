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

#include "LinkedListHeader.glsl"
#include "ColorPack.glsl"

out vec4 fragColor;

#define MAX_NUM_FRAGS 256

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
	LinkedListFragmentNode frags[MAX_NUM_FRAGS];
	LinkedListFragmentNode fragment;
	for (int i = 0; i < MAX_NUM_FRAGS; i++)
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
            if (fragment.depth < frags[index].depth)
            {
                LinkedListFragmentNode temp = fragment;
                fragment = frags[index];
                frags[index] = temp;
            }
        }
        frags[numFrags] = fragment;

		numFrags++;
	}

    // Blend all fragments in the right order
	for (int i = 0; i < numFrags; i++) {
		fragment = frags[i];

        // Back-to-Front (BTF)
		// Blend the accumulated color with the color of the fragment node
		/*float alpha = fragment.color.a;
		float alphaOut = alpha + color.a * (1.0 - alpha);
		color.rgb = (alpha * fragment.color.rgb + (1.0 - alpha) * color.a * color.rgb) / alphaOut;
		//color.rgb = (alpha * fragment.color.rgb + (1.0 - alpha) * color.a * color.rgb);
		color.a = alphaOut;*/

		// Front-to-Back (FTB)
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(fragment.color);
		float alphaSrc = colorSrc.a;
		color.rgb = color.rgb + (1.0 - color.a) * alphaSrc * colorSrc.rgb;
		color.a = color.a + (1.0 - color.a) * alphaSrc;
	}
	color = vec4(color.rgb / color.a, color.a);


	fragColor = color;
}