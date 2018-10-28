#ifndef PROCESS_VOXEL_GLSL
#define PROCESS_VOXEL_GLSL

float distanceSqr(vec3 v1, vec3 v2)
{
    return squareVec(v1 - v2);
}


#define MAX_NUM_HITS 16
struct RayHit {
    vec4 color;
    float distance;
    uint lineID;
};

void insertHitSorted(in RayHit insertHit, inout int numHits, inout RayHit hits[MAX_NUM_HITS])
{
    bool inserted = false;
    uint lineID = insertHit.lineID;
    int i;
    for (i = 0; i < numHits; i++) {
        if (insertHit.distance < hits[i].distance) {
            inserted = true;
            RayHit temp = insertHit;
            insertHit = hits[i];
            hits[i] = temp;
        }
        if (!inserted && hits[i].lineID == lineID) {
            return;
        }
        if (inserted && insertHit.lineID == lineID) {
            return;
        }
    }
    if (i != MAX_NUM_HITS) {
        hits[i] = insertHit;
        numHits++;
    }
}

/**
 * Processes the intersections with the geometry of the voxel at "voxelIndex".
 * Returns the color of the voxel (or completely transparent color if no intersection with geometry stored in voxel).
 */
void processVoxel(vec3 rayOrigin, vec3 rayDirection, ivec3 centerVoxelIndex, ivec3 voxelIndex,
        inout RayHit hits[MAX_NUM_HITS], inout int numHits, inout uint blendedLineIDs, inout uint nextBlendedLineIDs)
{
    vec3 centerVoxelPosMin = vec3(centerVoxelIndex);
    vec3 centerVoxelPosMax = vec3(centerVoxelIndex) + vec3(1);
    uint currVoxelNumLines = 0;
    LineSegment currVoxelLines[MAX_NUM_LINES_PER_VOXEL];
    loadLinesInVoxel(voxelIndex, currVoxelNumLines, currVoxelLines);

    for (int i = 0; i < currVoxelNumLines; i++) {
        uint lineID = currVoxelLines[i].lineID;
        uint lineBit = 1u << lineID;
        if ((blendedLineIDs & lineBit) != 0u) {
            continue;
        }

        bool hasIntersection = false;
        vec3 tubePoint1 = currVoxelLines[i].v1, tubePoint2 = currVoxelLines[i].v2;

        vec3 tubeIntersection, sphereIntersection1, sphereIntersection2;
        bool hasTubeIntersection, hasSphereIntersection1 = false, hasSphereIntersection2 = false;
        hasTubeIntersection = rayTubeIntersection(rayOrigin, rayDirection, tubePoint1, tubePoint2, lineRadius, tubeIntersection);
        hasSphereIntersection1 = raySphereIntersection(rayOrigin, rayDirection, tubePoint1, lineRadius, sphereIntersection1);
        hasSphereIntersection2 = raySphereIntersection(rayOrigin, rayDirection, tubePoint2, lineRadius, sphereIntersection2);

        // Get closest intersection point
        int closestIntersectionIdx = 0;
        vec3 intersection = tubeIntersection;
        float distTube = distanceSqr(rayOrigin, tubeIntersection);
        float distSphere1 = distanceSqr(rayOrigin, sphereIntersection1);
        float distSphere2 = distanceSqr(rayOrigin, sphereIntersection2);
        float dist = 1e7;
        if (hasTubeIntersection && distTube < dist
                /*&& all(greaterThanEqual(intersection, centerVoxelPosMin))
                && all(lessThanEqual(intersection, centerVoxelPosMax))*/) {
            closestIntersectionIdx = 0;
            intersection = tubeIntersection;
            dist = distTube;
            hasIntersection = true;
        }
        if (hasSphereIntersection1 && distSphere1 < dist
                /*&& all(greaterThanEqual(intersection, centerVoxelPosMin))
                && all(lessThanEqual(intersection, centerVoxelPosMax))*/) {
            closestIntersectionIdx = 1;
            intersection = sphereIntersection1;
            dist = distSphere1;
            hasIntersection = true;
        }
        if (hasSphereIntersection2 && distSphere2 < dist
                /*&& all(greaterThanEqual(intersection, centerVoxelPosMin))
                && all(lessThanEqual(intersection, centerVoxelPosMax))*/) {
            closestIntersectionIdx = 2;
            intersection = sphereIntersection2;
            dist = distSphere2;
            hasIntersection = true;
        }
        //if (any(lessThan(intersection, centerVoxelPosMin)) || any(greaterThan(intersection, centerVoxelPosMax))) {
        //    continue;
        //}

        if (hasIntersection) {
            vec3 closestIntersectionNormal;
            float closestIntersectionAttribute;
            if (closestIntersectionIdx == 0) {
                // Tube
                vec3 v = tubePoint2 - tubePoint1;
                vec3 u = intersection - tubePoint1;
                float t = dot(v, u) / dot(v, v);
                vec3 centerPt = tubePoint1 + t*v;
                closestIntersectionAttribute = (1-t)*currVoxelLines[i].a1 + t*currVoxelLines[i].a2;
                closestIntersectionNormal =  normalize(intersection - centerPt);
            } else {
                vec3 sphereCenter;
                if (closestIntersectionIdx == 1) {
                    sphereCenter = tubePoint1;
                    closestIntersectionAttribute = currVoxelLines[i].a1;
                } else {
                    sphereCenter = tubePoint2;
                    closestIntersectionAttribute = currVoxelLines[i].a2;
                }
                closestIntersectionNormal = normalize(intersection - sphereCenter);
            }


            RayHit hit;
            hit.distance = dist;

            // Evaluate lighting
            float diffuseFactor = clamp(dot(closestIntersectionNormal, lightDirection), 0.0, 1.0) + 0.5;
            vec4 diffuseColor = vec4(vec3(186.0, 106.0, 57.0) / 255.0, 1.0); // Test color w/o transfer function
            hit.color = vec4(diffuseColor.rgb * diffuseFactor, diffuseColor.a);

            const float occlusionFactor = 1.0;
            vec4 diffuseColorVorticity = transferFunction(closestIntersectionAttribute);
            vec3 diffuseShadingVorticity = diffuseColorVorticity.rgb * clamp(dot(closestIntersectionNormal,
                    lightDirection)/2.0 + 0.75 * occlusionFactor, 0.0, 1.0);
            hit.color = vec4(diffuseShadingVorticity, diffuseColorVorticity.a); // * globalColor.a
            hit.lineID = lineID;

            insertHitSorted(hit, numHits, hits);
            nextBlendedLineIDs |= lineBit;
        }
    }
}

/**
 * Processes the intersections with the geometry of the voxel at "voxelIndex".
 * Returns the color of the voxel (or completely transparent color if no intersection with geometry stored in voxel).
 */
vec4 nextVoxel(vec3 rayOrigin, vec3 rayDirection, ivec3 voxelIndex, inout uint blendedLineIDs)
{
    RayHit hits[MAX_NUM_HITS];
    int numHits = 0;

    /*
    uint currVoxelNumLines = 0;
    LineSegment currVoxelLines[MAX_NUM_LINES_PER_VOXEL];
    loadLinesInVoxel(voxelIndex, currVoxelNumLines, currVoxelLines);

    for (int i = 0; i < currVoxelNumLines; i++) {
        bool hasIntersection = false;
        vec3 tubePoint1 = currVoxelLines[i].v1, tubePoint2 = currVoxelLines[i].v2;

        vec3 tubeIntersection, sphereIntersection1, sphereIntersection2;
        bool hasTubeIntersection, hasSphereIntersection1 = false, hasSphereIntersection2 = false;
        hasTubeIntersection = rayTubeIntersection(rayOrigin, rayDirection, tubePoint1, tubePoint2, lineRadius, tubeIntersection);
        hasSphereIntersection1 = raySphereIntersection(rayOrigin, rayDirection, tubePoint1, lineRadius, sphereIntersection1);
        hasSphereIntersection2 = raySphereIntersection(rayOrigin, rayDirection, tubePoint2, lineRadius, sphereIntersection2);
        hasIntersection = hasTubeIntersection || hasSphereIntersection1 || hasSphereIntersection2;

        // Get closest intersection point
        int closestIntersectionIdx = 0;
        vec3 intersection = tubeIntersection;
        float distTube = distanceSqr(rayOrigin, tubeIntersection);
        float distSphere1 = distanceSqr(rayOrigin, sphereIntersection1);
        float distSphere2 = distanceSqr(rayOrigin, sphereIntersection2);
        float dist = 1e7;
        if (hasTubeIntersection && distTube < dist) {
            closestIntersectionIdx = 0;
            intersection = tubeIntersection;
            dist = distTube;
        }
        if (hasSphereIntersection1 && distSphere1 < dist) {
            closestIntersectionIdx = 1;
            intersection = sphereIntersection1;
            dist = distSphere1;
        }
        if (hasSphereIntersection2 && distSphere2 < dist) {
            closestIntersectionIdx = 2;
            intersection = sphereIntersection2;
            dist = distSphere2;
        }

        if (hasIntersection) {
            vec3 closestIntersectionNormal;
            float closestIntersectionAttribute;
            if (closestIntersectionIdx == 0) {
                // Tube
                vec3 v = tubePoint2 - tubePoint1;
                vec3 u = intersection - tubePoint1;
                float t = dot(v, u) / dot(v, v);
                vec3 centerPt = tubePoint1 + t*v;
                closestIntersectionAttribute = (1-t)*currVoxelLines[i].a1 + t*currVoxelLines[i].a2;
                closestIntersectionNormal =  normalize(intersection - centerPt);
                return vec4(vec3(1.0, mod(float(voxelIndex.x + voxelIndex.y + voxelIndex.z) * 0.39475587, 1.0), 0.0), 1.0); // Test
            } else {
                vec3 sphereCenter;
                if (closestIntersectionIdx == 1) {
                    sphereCenter = tubePoint1;
                    closestIntersectionAttribute = currVoxelLines[i].a1;
                } else {
                    sphereCenter = tubePoint2;
                    closestIntersectionAttribute = currVoxelLines[i].a2;
                }
                closestIntersectionNormal = normalize(intersection - sphereCenter);
                return vec4(vec3(0.9, mod(float(voxelIndex.x + voxelIndex.y + voxelIndex.z) * 0.39475587, 1.0), 0.0), 1.0); // Test
            }


            RayHit hit;
            hit.distance = dist;

            // Evaluate lighting
            float diffuseFactor = clamp(dot(closestIntersectionNormal, lightDirection), 0.0, 1.0) + 0.5;
            vec4 diffuseColor = vec4(vec3(186.0, 106.0, 57.0) / 255.0, 1.0); // Test color w/o transfer function
            hit.color = vec4(diffuseColor.rgb * diffuseFactor, diffuseColor.a);

            const float occlusionFactor = 1.0;
            vec4 diffuseColorVorticity = transferFunction(uint(round(255.0*closestIntersectionAttribute)));
            vec3 diffuseShadingVorticity = diffuseColorVorticity.rgb * clamp(dot(closestIntersectionNormal,
                    lightDirection)/2.0 + 0.75 * occlusionFactor, 0.0, 1.0);
            //hit.color = vec4(diffuseShadingVorticity, diffuseColorVorticity.a); // globalColor.a

            insertHitSorted(hit, numHits, hits);
        } else {
            return vec4(vec3(0.8, mod(float(voxelIndex.x + voxelIndex.y + voxelIndex.z) * 0.39475587, 1.0), 0.0), 0.2); // Test
        }

        //return vec4(vec3(1.0), texelFetch(densityTexture, voxelIndex, 0).r);
        //return vec4(vec3(1.0), texture(densityTexture, intersection/vec3(gridResolution-ivec3(1))).r);
    }*/

    uint nextBlendedLineIDs = blendedLineIDs;

    /*for (int i = 0; i <= 27; i++) {
        int x = i % 3 - 1;
        int y = (i / 3) % 3 - 1;
        int z = (i / 9) - 1;
        ivec3 processedVoxelIndex = voxelIndex + ivec3(x,y,z);
        if (all(greaterThanEqual(processedVoxelIndex, ivec3(0)))
                && all(lessThan(processedVoxelIndex, gridResolution))) {
            processVoxel(rayOrigin, rayDirection, voxelIndex, processedVoxelIndex, hits, numHits,
                    blendedLineIDs, nextBlendedLineIDs);
        }
    }*/
    /*for (int z = -1; z <= 1; z++) {
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                ivec3 processedVoxelIndex = voxelIndex + ivec3(x,y,z);
                if (all(greaterThanEqual(processedVoxelIndex, ivec3(0)))
                        && all(lessThan(processedVoxelIndex, gridResolution))) {
                    processVoxel(rayOrigin, rayDirection, voxelIndex, processedVoxelIndex, hits, numHits,
                            blendedLineIDs, nextBlendedLineIDs);
                }
            }
        }
    }*/
    processVoxel(rayOrigin, rayDirection, voxelIndex, voxelIndex, hits, numHits,
            blendedLineIDs, nextBlendedLineIDs);
    blendedLineIDs = nextBlendedLineIDs;

    vec4 color = vec4(0.0);
    for (int i = 0; i < numHits; i++) {
        if (blend(hits[i].color, color)) {
            return color; // Early ray termination
        }
    }

    return color;
}


#endif