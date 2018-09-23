
#include "PixelSyncHeader.glsl"
#include "ColorPack.glsl"
#include "TiledAddress.glsl"

#define REQUIRE_INVOCATION_INTERLOCK

out vec4 fragColor;

void gatherFragment(vec4 color)
{
	uint x = uint(gl_FragCoord.x);
	uint y = uint(gl_FragCoord.y);
	uint pixelIndex = addrGen(uvec2(x,y));
	// Fragment index (in nodes buffer):
	uint index = MAX_NUM_NODES*pixelIndex;

	FragmentNode frag;
	frag.color = packColorRGBA(color);
	frag.depth = gl_FragCoord.z;

	memoryBarrierBuffer();

	// Use insertion sort to insert new fragment
	uint numFragments = numFragmentsBuffer[pixelIndex];
	for (uint i = 0; i < numFragments; i++)
	{
		if (frag.depth < nodes[index].depth)
		{
			FragmentNode temp = frag;
			frag = nodes[index];
			nodes[index] = temp;
		}
		index++;
	}
	
	// Store the fragment at the end of the list if capacity is left
	if (numFragments < MAX_NUM_NODES) {
		atomicAdd(numFragmentsBuffer[pixelIndex], 1);
		//numFragmentsBuffer[pixelIndex]++;
		nodes[index] = frag;
		//atomicCounterIncrement(numFragmentsBuffer[pixelIndex]);
		//numFragmentsBuffer[pixelIndex]++; // Race conditon on Intel if used here. Why?
	}

	memoryBarrierBuffer();
	
	// If no space was left to store the last fragment, simply discard it.
	// TODO: Merge nodes with least visual impact?
	/*if (frag.used == 1) {
		int lastIndex = index-1;
	
		// Blend with last fragment
		float alpha = nodes[lastIndex].color.a;
		float alphaOut = alpha + frag.color.a * (1.0 - alpha);
		nodes[lastIndex].color.rgb = (alpha * nodes[lastIndex].color.rgb + (1.0 - alpha) * frag.color.a * frag.color.rgb) / alphaOut;
		//color.rgb = (alpha * nodes[lastIndex].color.rgb + (1.0 - alpha) * frag.color.a * frag.color.rgb);
		nodes[lastIndex].color.a = alphaOut;
	}*/
		
	fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
