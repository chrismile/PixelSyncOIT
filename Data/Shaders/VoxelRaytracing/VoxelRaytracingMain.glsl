-- Compute

#version 430

layout (local_size_x = 8, local_size_y = 4, local_size_z = 1) in;


// Output of rasterizer
layout(rgba32f, binding = 0) uniform image2D imageOutput;


// Size of the rendering viewport (/window)
uniform ivec2 viewportSize;

// Camera data
uniform vec3 cameraPositionWorld;
uniform float aspectRatio;
uniform float fov;

// Convert points in world space to voxel space (voxel grid at range (0, 0, 0) to (rx, ry, rz)).
uniform mat4 worldSpaceToVoxelSpace;
uniform mat4 inverseViewMatrix;

uniform vec4 clearColor = vec4(0.0);


#include "CollisionDetection.glsl"
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
	if (length(vec2(fragCoord)) < 200.0f) {
		//fragColor = vec4(0.0, 1.0, 0.0, 1.0);
	}

	/*vec2 centeredWindowCoords = vec2(fragCoord) - vec2(viewportSize) / 2.0;
	vec3 rayOrigin = vec3(centeredWindowCoords, 0.0);
	vec3 rayDirection = vec3(0.0, 0.0, -1.0);

	float tubeRadius = 100.0f;

	vec3 sphereCenter = vec3(-300.0, 0.0, -10.0);
	vec3 intersectionPosition;
	if (raySphereIntersection(rayOrigin, rayDirection, sphereCenter, tubeRadius, intersectionPosition)) {
		fragColor = vec4(0.0, 1.0, 0.0, 1.0);
	}

	vec3 tubeStart = vec3(200.0, -200.0, -10.0);
	vec3 tubeEnd = vec3(300.0, 100.0, -10.0);
	if (rayTubeIntersection(rayOrigin, rayDirection, tubeStart, tubeEnd, tubeRadius, intersectionPosition)) {
		fragColor = vec4(0.0, 1.0, 0.0, 1.0);
	}

    vec3 boxPosition = vec3(0.0, 0.0, -10);
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
        /*if (boxContainsPoint(rayOrigin, voxelGridLower, voxelGridUpper)) {
            entrancePoint = rayOrigin;
        }
		fragColor = traverseVoxelGrid(rayOrigin, rayDirection, entrancePoint, exitPoint);*/
		/*if (tNear > 0.0) {
            fragColor = vec4(0.0, 0.0, 1.0, 1.0);
		} else {
            fragColor = vec4(1.0, 0.0, 1.0, 1.0);
		}*/
		// First intersection point behind camera ray origin?
		vec3 entrancePoint = rayOrigin + tNear * rayDirection;
        vec3 exitPoint = rayOrigin + tFar * rayDirection;
		if (tNear < 0.0) {
            entrancePoint = rayOrigin;
        }
        fragColor = traverseVoxelGrid(rayOrigin, rayDirection, entrancePoint, exitPoint);
	}

	imageStore(imageOutput, fragCoord, fragColor);
}
