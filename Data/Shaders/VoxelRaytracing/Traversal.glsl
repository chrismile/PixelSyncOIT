#ifndef TRAVERSAL_GLSL
#define TRAVERSAL_GLSL

/**
 * Code inspired by "A Fast Voxel Traversal Algorithm for Ray Tracing" written by John Amanatides, Andrew Woo.
 * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.42.3443&rep=rep1&type=pdf
 * Value initialization code adapted from: https://stackoverflow.com/questions/12367071/how-do-i-initialize-the-t-
 * variables-in-a-fast-voxel-traversal-algorithm-for-ray
 *
 * Traverses the voxel grid from "startPoint" until "endPoint".
 * Calls "nextVoxel" each time a voxel is entered.
 * Returns the accumulated color using voxel raytracing.
 */
vec4 traverseVoxelGrid(vec3 rayOrigin, vec3 rayDirection, vec3 startPoint, vec3 endPoint)
{
    vec4 color = vec4(0.0);

    // Bit-mask for already blended lines
    uint blendedLineIDs = 0;
    uint oldBlendedLineIDs1 = 0;
    uint oldBlendedLineIDs2 = 0;


    float tMaxX, tMaxY, tMaxZ, tDeltaX, tDeltaY, tDeltaZ;
    ivec3 voxelIndex;

    int stepX = int(sign(endPoint.x - startPoint.x));
    if (stepX != 0)
        tDeltaX = min(stepX / (endPoint.x - startPoint.x), 1e7);
    else
        tDeltaX = 1e7; // inf
    if (stepX > 0)
        tMaxX = tDeltaX * (1.0 - fract(startPoint.x));
    else
        tMaxX = tDeltaX * fract(startPoint.x);
    voxelIndex.x = int(startPoint.x);

    int stepY = int(sign(endPoint.y - startPoint.y));
    if (stepY != 0)
        tDeltaY = min(stepY / (endPoint.y - startPoint.y), 1e7);
    else
        tDeltaY = 1e7; // inf
    if (stepY > 0)
        tMaxY = tDeltaY * (1.0 - fract(startPoint.y));
    else
        tMaxY = tDeltaY * fract(startPoint.y);
    voxelIndex.y = int(startPoint.y);

    int stepZ = int(sign(endPoint.z - startPoint.z));
    if (stepZ != 0)
        tDeltaZ = min(stepZ / (endPoint.z - startPoint.z), 1e7);
    else
        tDeltaZ = 1e7; // inf
    if (stepZ > 0)
        tMaxZ = tDeltaZ * (1.0 - fract(startPoint.z));
    else
        tMaxZ = tDeltaZ * fract(startPoint.z);
    voxelIndex.z = int(startPoint.z);

    if (stepX == 0 && stepY == 0 && stepZ == 0) {
        return vec4(0.0);
    }

    if (getNumLinesInVoxel(voxelIndex) > 0) {
        vec4 voxelColor = nextVoxel(rayOrigin, rayDirection, voxelIndex, blendedLineIDs);
        if (blendPremul(voxelColor, color)) {
            // Early ray termination
            return color;
        }
    }

    int iterationNum = 0;
    while (true) {
        /*int lodIndex
        int numNodes = texelFetch(octreeTexture, pos, lodIndex);
        if (numNodes == 0) {
            // Skip
        } else {
            ;
        }*/

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                voxelIndex.x += stepX;
                tMaxX += tDeltaX;
            } else {
                voxelIndex.z += stepZ;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                voxelIndex.y += stepY;
                tMaxY += tDeltaY;
            } else {
                voxelIndex.z += stepZ;
                tMaxZ += tDeltaZ;
            }
        }
        //if (!(any(lessThan(voxelIndex, ivec3(0))) || any(greaterThanEqual(voxelIndex, gridResolution)))
        //        && (tMaxX > 1.0 && tMaxY > 1.0 && tMaxZ > 1.0))
        //    return vec4(vec3(1.0, 0.0, 0.0), 1.0);

        //if (tMaxX > 1.0 && tMaxY > 1.0 && tMaxZ > 1.0)
        //    break;
        if (any(lessThan(voxelIndex, ivec3(0))) || any(greaterThanEqual(voxelIndex, gridResolution)))
            break;

        if (getNumLinesInVoxel(voxelIndex) > 0) {
            vec4 voxelColor = nextVoxel(rayOrigin, rayDirection, voxelIndex, blendedLineIDs);
            iterationNum++;
            oldBlendedLineIDs1 = blendedLineIDs;
            blendedLineIDs &= ~oldBlendedLineIDs2;
            oldBlendedLineIDs2 = oldBlendedLineIDs1;
            if (blendPremul(voxelColor, color)) {
                // Early ray termination
                return color;
            }
            //return vec4(vec3(1.0), 1.0);
        }
    }

    return color;
}

#endif