-- Compute

#version 430

layout (local_size_x = 64, local_size_y = 4, local_size_z = 1) in;

#include "VoxelData.glsl"
#include "TransferFunction.glsl"

layout (binding = 0, r32f) uniform image3D densityImage;

void main() {
    ivec3 voxelIndex = gl_GlobalInvocationID.xyz;
    if (any(greaterThanEqual(voxelIndex, gridResolution))) {
        return;
    }
    uint voxelIndex1D = getVoxelIndex1D(voxelIndex);

    uint lineOffset = getLineListOffset(voxelIndex1D);
    uint numLinePointsVoxel = getNumLinesInVoxel(voxelIndex1D);
    int numLinePoints = max(int(numLinePointsVoxel), MAX_NUM_LINES_PER_VOXEL);

    float density = 0.0;
    LineSegment lineSegment;
    for (int i = 0; i < numLinePoints; i++) {
        decompressLine(vec3(voxelIndex), lineSegments[lineOffset+i], lineSegment);
        density += line.length() * line.avgOpacity(maxVorticity);
        float lineLength = length(lineSegment.v2 - lineSegment.v1);
        #ifdef HAIR_RENDERING
        density += lineLength * hairStrandColor.a;
        #else
        density += lineLength * (transferFunction(lineSegment.a1).a + transferFunction(lineSegment.a2).a) / 2.0f;
        #endif
    }
    imageStore(densityImage, voxelIndex, vec4(density));
}
