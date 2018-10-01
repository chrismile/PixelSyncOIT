
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

#ifndef TEST_NO_ATOMIC_OPERATIONS
	uint insertIndex = atomicCounterIncrement(fragCounter);
#else
	uint insertIndex = fragCounter++;
#endif

    if (insertIndex < linkedListSize) {
    	// Insert the fragment into the linked list
#ifndef TEST_NO_ATOMIC_OPERATIONS
        frag.next = atomicExchange(startOffset[pixelIndex], insertIndex);
#else
        frag.next = startOffset[pixelIndex];
        startOffset[pixelIndex] = insertIndex;
#endif
        fragmentBuffer[insertIndex] = frag;
    }
}
