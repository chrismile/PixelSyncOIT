
#include "LinkedListHeader.glsl"
#include "ColorPack.glsl"

out vec4 fragColor;

void gatherFragment(vec4 color)
{
	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);
	int pixelIndex = viewportW*y + x;

	LinkedListFragmentNode frag;
	frag.color = packColorRGBA(color);
	frag.depth = gl_FragCoord.z;
	frag.next = -1;

	uint insertIndex = atomicCounterIncrement(fragCounter);
    if (insertIndex < linkedListSize) {
    	// Insert the fragment into the linked list
        frag.next = atomicExchange(startOffset[pixelIndex], insertIndex);
        fragmentBuffer[insertIndex] = frag;
    }
}
