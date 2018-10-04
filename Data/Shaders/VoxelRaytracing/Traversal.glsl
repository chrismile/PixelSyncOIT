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

    // 1. Initialize the following variables
    float tMaxX, tMaxY, tMaxZ, tDeltaX, tDeltaY, tDeltaZ;
    ivec3 voxelIndex;

    int dx = int(sign(endPoint.x - startPoint.x));
    if (dx != 0)
        tDeltaX = min(dx / (endPoint.x - startPoint.x), 1e7);
    else
        tDeltaX = 1e7; // inf
    if (dx > 0)
        tMaxX = tDeltaX * (1.0 - fract(startPoint.x));
    else
        tMaxX = tDeltaX * fract(startPoint.x);
    voxelIndex.x = int(startPoint.x);

    int dy = int(sign(endPoint.y - startPoint.y));
    if (dy != 0)
        tDeltaY = min(dy / (endPoint.y - startPoint.y), 1e7);
    else
        tDeltaY = 1e7; // inf
    if (dy > 0)
        tMaxY = tDeltaY * (1.0 - fract(startPoint.y));
    else
        tMaxY = tDeltaY * fract(startPoint.y);
    voxelIndex.y = int(startPoint.y);

    int dz = int(sign(endPoint.z - startPoint.z));
    if (dz != 0)
        tDeltaZ = min(dz / (endPoint.z - startPoint.z), 1e7);
    else
        tDeltaZ = 1e7; // inf
    if (dz > 0)
        tMaxZ = tDeltaZ * (1.0 - fract(startPoint.z));
    else
        tMaxZ = tDeltaZ * fract(startPoint.z);
    voxelIndex.z = int(startPoint.z);

    while (true) {
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                voxelIndex.x += dx;
                tMaxX += tDeltaX;
            } else {
                voxelIndex.z += dz;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                voxelIndex.y += dy;
                tMaxY += tDeltaY;
            } else {
                voxelIndex.z += dz;
                tMaxZ += tDeltaZ;
            }
        }
        if (tMaxX > 1.0 && tMaxY > 1.0 && tMaxZ > 1.0)
            break;

        loadLinesInVoxel(voxelIndex);
        if (currVoxelNumLines > 0) {
            vec4 voxelColor = nextVoxel(rayOrigin, rayDirection, voxelIndex);
            if (blend(voxelColor, color)) {
                // Early ray termination
                return color;
            }
        }
    }

    return color;
}

#endif