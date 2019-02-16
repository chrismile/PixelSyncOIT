-- Compute

#version 430

layout (local_size_x = 8, local_size_y = 4, local_size_z = 1) in;


// Output of rasterizer
layout(rgba8, binding = 0) writeonly uniform image2D imageOutput;


// Size of the rendering viewport (/window)
uniform ivec2 viewportSize;

// Camera data
uniform vec3 cameraPositionWorld;
uniform float aspectRatio;
uniform float fov;

// Convert points in world space to voxel space (voxel grid at range (0, 0, 0) to (rx, ry, rz)).
uniform mat4 worldSpaceToVoxelSpace;
uniform mat4 voxelSpaceToWorldSpace;
uniform mat4 inverseViewMatrix;

uniform vec4 clearColor = vec4(0.0);

// Global lighting data
uniform vec3 lightDirection = vec3(1.0, 0.0, 0.0);

#ifdef HAIR_RENDERING
uniform vec4 hairStrandColor = vec4(1.0, 0.0, 0.0, 1.0);
#endif

#include "CollisionDetection.glsl"
#include "TransferFunction.glsl"
#include "Blend.glsl"
#include "VoxelData.glsl"
#include "ProcessVoxel.glsl"
#include "Traversal.glsl"


void main()
{
    uvec2 globalID = gl_GlobalInvocationID.xy;
    if (globalID.x >= viewportSize.x || globalID.y >= viewportSize.y) {
        // Outside of viewport
        return;
    }

    ivec2 fragCoord = ivec2(globalID);
    vec4 fragColor = clearColor;

    // Testing code
    /*if (length(vec2(fragCoord)) < 200.0f) {
        //fragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }

    vec2 centeredWindowCoords = vec2(fragCoord) - vec2(viewportSize) / 2.0;
    vec3 rayOrigin = vec3(centeredWindowCoords, 0.0);
    vec3 rayDirection = vec3(0.0, 0.0, -1.0);

    float tubeRadius = 100.0f;
    bool hasIntersection = false;

    vec3 tubePoint1 = vec3(-300.0, -49.0, -200.0);
    vec3 tubePoint2 = vec3(300.0, 100.0, -150.0);
    vec3 tubeIntersection, sphereIntersection1, sphereIntersection2;
    bool hasTubeIntersection, hasSphereIntersection1, hasSphereIntersection2;
    hasSphereIntersection1 = raySphereIntersection(rayOrigin, rayDirection, tubePoint1, tubeRadius, sphereIntersection1);
    hasSphereIntersection2 = raySphereIntersection(rayOrigin, rayDirection, tubePoint2, tubeRadius, sphereIntersection2);
    hasTubeIntersection = rayTubeIntersection(rayOrigin, rayDirection, tubePoint1, tubePoint2, tubeRadius, tubeIntersection);
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
            //closestIntersectionAttribute = (1-t)*currVoxelLines[i].a1 + t*currVoxelLines[i].a2; // TODO
            closestIntersectionNormal =  normalize(intersection - centerPt);
            //fragColor = vec4(vec3(1.0, 0.0, 0.0), 1.0);
        } else {
            vec3 sphereCenter;
            if (closestIntersectionIdx == 1) {
                sphereCenter = tubePoint1;
                //closestIntersectionAttribute = currVoxelLines[i].a1;
            } else {
                sphereCenter = tubePoint2;
                //closestIntersectionAttribute = currVoxelLines[i].a2;
            }
            closestIntersectionNormal = normalize(intersection - sphereCenter);
            //fragColor = vec4(vec3(clamp(dot(closestIntersectionNormal, lightDirection), 0.0, 1.0) + 0.5), 1.0);
            //fragColor = vec4(vec3(0.0, 1.0, 0.0), 1.0);
        }
        //fragColor = vec4(vec3(sqrt(dist)/200.0f), 1.0);

        // Evaluate lighting
        fragColor = vec4(vec3(clamp(dot(closestIntersectionNormal, lightDirection), 0.0, 1.0) + 0.5), 1.0);
    }*/



    /*vec3 boxPosition = vec3(0.0, 0.0, -10);
    vec3 lower = boxPosition + vec3(-100, -100, -10);
    vec3 upper = boxPosition + vec3(100, 100, 0);
    vec3 entrancePoint;
    vec3 exitPoint;
    if (rayBoxIntersection(rayOrigin, rayDirection, lower, upper, entrancePoint, exitPoint)) {
        fragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }*/




    vec3 rayOrigin = (worldSpaceToVoxelSpace * inverseViewMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec2 rayDirCameraSpace;
    float scale = tan(fov * 0.5);
    rayDirCameraSpace.x = (2.0 * (float(fragCoord.x) + 0.5) / float(viewportSize.x) - 1) * aspectRatio * scale;
    rayDirCameraSpace.y = (2.0 * (float(fragCoord.y) + 0.5) / float(viewportSize.y) - 1) * scale;
    vec3 rayDirection = normalize((worldSpaceToVoxelSpace * inverseViewMatrix * vec4(rayDirCameraSpace, -1.0, 0.0)).xyz);

    // Get the intersections with the voxel grid
    vec3 voxelGridLower = vec3(0.0);
    vec3 voxelGridUpper = vec3(gridResolution);
    float tNear, tFar;
    if (rayBoxIntersectionRayCoords(rayOrigin, rayDirection, voxelGridLower, voxelGridUpper, tNear, tFar)) {
        // First intersection point behind camera ray origin?
        vec3 entrancePoint = rayOrigin + tNear * rayDirection + rayDirection*0.1;
        vec3 exitPoint = rayOrigin + tFar * rayDirection - rayDirection*0.1;
        if (tNear < 0.0) {
            entrancePoint = rayOrigin;
        }
        fragColor = traverseVoxelGrid(rayOrigin, rayDirection, entrancePoint, exitPoint);
        blend(clearColor, fragColor);
        fragColor = vec4(fragColor.rgb / fragColor.a, fragColor.a);
    }

    imageStore(imageOutput, fragCoord, fragColor);
}
