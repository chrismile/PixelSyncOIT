#ifndef PROCESS_VOXEL_GLSL
#define PROCESS_VOXEL_GLSL

float distanceSqr(vec3 v1, vec3 v2)
{
    return squareVec(v1 - v2);
}


#define MAX_NUM_HITS 8
struct RayHit {
    vec4 color;
    float distance;
};

void insertHitSorted(in RayHit insertHit, inout int numHits, inout RayHit hits[MAX_NUM_HITS])
{
    int i;
    for (i = 0; i < numHits; i++) {
        if (insertHit.distance < hits[i].distance) {
            RayHit temp = insertHit;
            insertHit = hits[i];
            hits[i] = temp;
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
vec4 nextVoxel(vec3 rayOrigin, vec3 rayDirection, ivec3 voxelIndex)
{
    RayHit hits[MAX_NUM_HITS];
    int numHits = 0;

    // Bit-mask for used lines
    uint blendedLineIDs = 0;

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
    }

    /*
    if (hasIntersection) {
        vec4 baseColor = mix(vec4(1.0, 1.0, 1.0, 0.0), vec4(1.0, 0.0, 0.0, 1.0), closestIntersectionTransparency);
        float diffuseFactor = dot(closestIntersectionNormal, rayDirection);
    }
    */
    //return vec4(vec3(0.0), 1.0);

    vec4 color = vec4(0.0);
    for (int i = 0; i < numHits; i++) {
        if (blend(hits[i].color, color)) {
            return color; // Early ray termination
        }
    }

    return color;
}

#endif