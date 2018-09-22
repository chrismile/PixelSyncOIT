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

vec4 blendFTB(uint fragsCount)
{
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

    return blendFTB(fragsCount);
}


vec4 insertionSort(uint fragsCount)
{
    // Temporary fragment storage
    uint fragColor;
    float fragDepth;

    uint i, j;
    for (i = 1; i < fragsCount; ++i) {
        // Get the fragment
        fragColor = colorList[i];
        fragDepth = depthList[i];

        j = i; // Store its position
        while (j >= 1 && depthList[j-1] > fragDepth) {
            // Shift the fragments through the list until place is found
            colorList[j] = colorList[j-1];
            depthList[j] = depthList[j-1];
            --j;
        }

        // Insert it at the right place
        colorList[j] = fragColor;
        depthList[j] = fragDepth;
    }

    return blendFTB(fragsCount);
}


vec4 shellSort(uint fragsCount)
{
    // Temporary fragment storage
    uint fragColor;
    float fragDepth;

    // Optimal gap sequence for 128 elements from [Ciu01, table 1]
    uint i, j, gap;
    uvec4 gaps = uvec4(24, 9, 4, 1);
    for(uint g = 0; g < 4; g++) {
        // For every gap
        gap = gaps[g]; // Current Cap
        for(i = gap; i < fragsCount; ++i) {
            // Get the fragment
            fragColor = colorList[i];
            fragDepth = depthList[i];
            j = i;

            // Shift earlier until correct
            while (j >= gap && depthList[j-gap] > fragDepth) {
                // Shift the fragments through the list until place is found
                colorList[j] = colorList[j-gap];
                depthList[j] = depthList[j-gap];
                j-=gap;
            }

            // Insert it at the right place
            colorList[j] = fragColor;
            depthList[j] = fragDepth;
        }
    }

    return blendFTB(fragsCount);
}


void maxHeapSink(uint x, uint fragsCount)
{
    uint c; // Child
    while((c = 2 * x + 1) < fragsCount) {
        // While children exist
        if(c + 1 < fragsCount && depthList[c] < depthList[c+1]) {
            // Find the biggest of both
            ++c;
        }

        if(depthList[x] >= depthList[c]) {
            // Does it have to sink
            return;
        } else {
            swapFrags(x, c);
            x = c; // Swap and sink again
        }
    }
}

vec4 heapSort(uint fragsCount)
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

    return blendFTB(fragsCount);
}


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
        // FTB Blending
		rayColor.rgb = rayColor.rgb + (1.0 - rayColor.a) * colorSrc.a * colorSrc.rgb;
		rayColor.a = rayColor.a + (1.0 - rayColor.a) * colorSrc.a;

        // Move Fragments up for next run
        colorList[0] = colorList[fragsCount-i];
        depthList[0] = depthList[fragsCount-i];
    }

    rayColor.rgb = rayColor.rgb / rayColor.a; // Correct rgb with alpha
    return rayColor;
}

