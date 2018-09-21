/**
 * This code was written by Maximilian Bandle for his bachelor's thesis "Opacity-Based Rendering of Lagrangian Particle
 * Trajectories in Met.3D". Most of his code was published under the GNU General Public License for the 3D visualization
 * software Met.3D (https://met3d.readthedocs.io/en/latest/).
 */


// Swap two Frags in color and depth Array => Avoid bacause expensive
void swapFrags(uint i, uint j) {
    uint cTemp = colorList[i];
    colorList[i] = colorList[j];
    colorList[j] = cTemp;
    float dTemp = depthList[i];
    depthList[i] = depthList[j];
    depthList[j] = dTemp;
}


vec4 bubbleSort(uint fragsCount)
{
    bool changed; // Has anything changed yet
    do {
        changed = false; // Nothing changed yet
        for (uint i = 0; i < fragsCount - 1; ++i) {
            // Go through all
            if(depthList[i] > depthList[i+1]) {
                // Order not correct => Swap
                swapFrags(i, i+1);
                changed = true; // Something has changed
            }
        }
    } while (changed); // Nothing changed => sorted

    vec4 color = vec4(0.0);
	for (uint i = 0; i < fragsCount; i++) {
		// Front-to-Back (FTB) blending
		// Blend the accumulated color with the color of the fragment node
		vec4 colorSrc = unpackColorRGBA(colorList[i]);
		float alphaSrc = colorSrc.a;
		color.rgb = color.rgb + (1.0 - color.a) * alphaSrc * colorSrc.rgb;
		color.a = color.a + (1.0 - color.a) * alphaSrc;
	}
	return vec4(color.rgb / color.a, color.a);
}

/*void insertionSort(uint fragsCount)
{
    uint i, j;
    LinkedListFragmentNode frag; // Temporary fragment storage
    for (i = 1; i < fragsCount; ++i) {
        frag = fragList[i]; // Get the fragment
        j = i; // Store its position
        while (j >= 1 && fragList[j-1].depth > frag.depth) {
            // Shift the fragments through the list until place is found
            fragList[j] = fragList[j-1];
            --j;
        }

        // Insert it at the right place
        fragList[j] = frag;
    }
}

void shellSort(uint fragsCount)
{
    uint i, j, gap;
    LinkedListFragmentNode frag; // Temporary fragment storage

    // Optimal gap sequence for 128 elements from [Ciu01, table 1]
    uvec4 gaps = uvec4(24, 9, 4, 1);
    for(uint g = 0; g < 4; g++) {
        // For every gap
        gap = gaps[g]; // Current Cap
        for(i = gap; i < fragsCount; ++i) {
            frag = fragList[i]; // Get the fragment
            j = i;

            // Shift earlier until correct
            while (j >= gap && fragList[j-gap].depth > frag.depth) {
                // Shift the fragments through the list until place is found
                fragList[j] = fragList[j-gap];
                j-=gap;
            }
            fragList[j] = frag; // Insert it at the right place
        }
    }
}

void maxHeapSink(uint x, uint fragsCount)
{
    uint c; // Child
    while((c = 2 * x + 1) < fragsCount) {
        // While children exist
        if(c + 1 < fragsCount && fragList[c].depth < fragList[c+1].depth) {
            // Find the biggest of both
            ++c;
        }

        if(fragList[x].depth >= fragList[c].depth) {
            // Does it have to sink
            return;
        } else {
            swapFrags(x, c);
            x = c; // Swap and sink again
        }
    }
}

void heapSort(uint fragsCount)
{
    uint i;
    for (i = (fragsCount + 1)/2 ; i > 0 ; --i) {
        // Bring it to heap structure
        maxHeapSink(i-1, fragsCount); // Sink all inner nodes
    }
    // Heap => Sorted List
    for (i=1;i<fragsCount;++i) {
        swapFrags(0, fragsCount-i); // Swap max to List End
        maxHeapSink(0, fragsCount-i); // Sink the max to obtain correct heap
    }
}*/


void minHeapSink4(uint x, uint fragsCount)
{
    uint c, t; // Child, Tmp
    while ((t = 4 * x + 1) < fragsCount) {
        if (t + 1 < fragsCount && depthList[t] > depthList[t+1]) {
            // 1st vs 2nd
            c = t + 1;
        } else {
            c = t;
        }

        if (t + 2 < fragsCount && depthList[c] > depthList[t+2]) {
            // Smallest vs 3rd
            c = t + 2;
        }

        if (t + 3 < fragsCount && depthList[c] > depthList[t+3]) {
            // Smallest vs 3rd
            c = t + 3;
        }

        if (depthList[x] <= depthList[c]) {
            return;
        } else {
            swapFrags(x, c);
            x = c;
        }
    }
}


vec4 frontToBackPQ(uint fragsCount)
{
    uint i;

    // Bring it to heap structure
    for (i = fragsCount/4; i > 0; --i) {
        // First is not one right place - will be done in for
        minHeapSink4(i, fragsCount); // Sink all inner nodes
    }

    // Start with transparent Ray
    vec4 rayColor = vec4(0.0);
    i = 0;
    while (i < fragsCount && rayColor.a < 0.99) {
        // Max Steps = #frags Stop when color is saturated enough
        minHeapSink4(0, fragsCount - i++); // Sink it right + increment i
        vec4 colorSrc = unpackColorRGBA(colorList[0]); // Heap first is min
        //vec4 colorSrc = unpackUnorm4x8(colorList[0]); // Heap first is min
        // FTB Blending
        //rayColor.rgb += (1-rayColor.a)*color.a*color.rgb;
        //rayColor.a += color.a*(1-rayColor.a);
		rayColor.rgb = rayColor.rgb + (1.0 - rayColor.a) * colorSrc.a * colorSrc.rgb;
		rayColor.a = rayColor.a + (1.0 - rayColor.a) * colorSrc.a;

        // Move Fragments up for next run
        colorList[0] = colorList[fragsCount-i];
        depthList[0] = depthList[fragsCount-i];
    }
    //vec4 colorSrc = unpackColorRGBA(colorList[0]);
    //rayColor.rgb = rayColor.rgb + (1.0 - rayColor.a) * colorSrc.a * colorSrc.rgb;
    //rayColor.a = rayColor.a + (1.0 - rayColor.a) * colorSrc.a;


    rayColor.rgb = rayColor.rgb / rayColor.a; // Correct rgb with alpha
    return rayColor;
}

#define sortingAlgorithm frontToBackPQ
//#define sortingAlgorithm bubbleSort

/*shader FSblend(out vec4 fragColor)
{
    // Receive the current head Pointer
    uint headPtr = imageLoad(headIndex, ivec2(gl_FragCoord.xy)).r;
    if(headPtr == 0x00) {
        // When unchanged => Transparent Fragment
        fragColor = vec4(0.0);
    } else {
        uint fragsCount = collectFragments(headPtr);
        fragColor = frontToBackPQ(fragsCount);
    }
}*/