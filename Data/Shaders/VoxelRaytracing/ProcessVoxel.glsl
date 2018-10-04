#ifndef PROCESS_VOXEL_GLSL
#define PROCESS_VOXEL_GLSL

float distanceSqr(vec3 v1, vec3 v2)
{
    return squareVec(v1 - v2);
}

/**
 * Processes the intersections with the geometry of the voxel at "voxelIndex".
 * Returns the color of the voxel (or completely transparent color if no intersection with geometry stored in voxel).
 */
vec4 nextVoxel(vec3 rayOrigin, vec3 rayDirection, ivec3 voxelIndex)
{
    int numLines = 0;
    for (int i = 0; i < currVoxelNumLines; i++) {
        vec3 tubePoint1 = currVoxelLines[i].v1, tubePoint2 = currVoxelLines[i].v2;

        vec3 tubeIntersection, sphereIntersection1, sphereIntersection2;
        rayTubeIntersection(rayOrigin, rayDirection, tubePoint1, tubePoint2, TUBE_RADIUS, tubeIntersection);
        raySphereIntersection(rayOrigin, rayDirection, tubePoint1, TUBE_RADIUS, sphereIntersection1);
        raySphereIntersection(rayOrigin, rayDirection, tubePoint2, TUBE_RADIUS, sphereIntersection2);

        // Get closest intersection point
        vec3 intersection = tubeIntersection;
        float dist = distanceSqr(rayOrigin, tubeIntersection);
        float newDist;
        newDist = distanceSqr(rayOrigin, sphereIntersection1);
        if (newDist < dist) {
            intersection = sphereIntersection1;
            dist = newDist;
        }
        newDist = distanceSqr(rayOrigin, sphereIntersection2);
        if (newDist < dist) {
            intersection = sphereIntersection2;
            dist = newDist;
        }

        // Evaluate lighting
        // TODO
        return vec4(1.0);
    }
    return vec4(0.0);
}

#endif