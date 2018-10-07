#ifndef VOXEL_DATA_GLSL
#define VOXEL_DATA_GLSL

#define GRID_RESOLUTION 256
#define QUANTIZATION_RESOLUTION 8
#define QUANTIZATION_RESOLUTION_LOG2 3
#define TUBE_RADIUS 0.2
#define MAX_NUM_LINES_PER_VOXEL 8
#define gridResolution ivec3(256, 256, 256)

struct LineSegment
{
	vec3 v1; // Vertex position
	float a1; // Vertex attribute
	vec3 v2; // Vertex position
	float a2; // Vertex attribute
};

struct LineSegmentCompressed
{
    // Bit 0-2, 3-5: Face ID of start/end point.
    // For c = log2(QUANTIZATION_RESOLUTION^2) = 2*log2(QUANTIZATION_RESOLUTION):
    // Bit 6-(5+c), (6+c)-(5+2c): Quantized face position of start/end point.
	uint linePosition;
	// Bit 0-15, 16-31: Attribute of start point (normalized to [0,1]).
	uint attributes;
};

// Offset of voxels in buffer above
layout (std430, binding = 0) readonly buffer VoxelLineListOffsetBuffer
{
	uint voxelLineListOffsets[];
};

// Offset of voxels in buffer above
layout (std430, binding = 1) readonly buffer NumLinesBuffer
{
	uint numLinesInVoxel[];
};

// Buffer containing all line segments
layout (std430, binding = 2) readonly buffer LineSegmentBuffer
{
#ifdef PACK_LINES
	LineSegmentCompressed lineSegments[];
#else
	LineSegment lineSegments[];
#endif
};


// Density of voxels (with LODs)
uniform sampler3D densityTexture;


// --- Data global to shader invocation ---

uint currVoxelNumLines = 0;
LineSegment currVoxelLines[MAX_NUM_LINES_PER_VOXEL];


// --- Functions ---

#ifdef PACK_LINES
void decompressLine(in LineSegmentCompressed compressedLine, out LineSegment decompressedLine)
{
    const int c = 2*QUANTIZATION_RESOLUTION_LOG2;
    uint faceStartID = compressedLine.linePosition & 0x7u;
    uint faceEndID = (compressedLine.linePosition >> 3) & 0x7u;
    uint quantizedStartPos1D = (compressedLine.linePosition >> 6) & 0x7u;
    uint quantizedEndPos1D = (compressedLine.linePosition >> 3) & 0x7u;
}
#endif

/// Stores lines of voxel in global variables "currVoxelLines", "currVoxelNumLines"
void loadLinesInVoxel(ivec3 voxelIndex)
{
    int voxelIndex1D = voxelIndex.x + voxelIndex.y*GRID_RESOLUTION + voxelIndex.z*GRID_RESOLUTION*GRID_RESOLUTION;
    currVoxelNumLines = numLinesInVoxel[voxelIndex1D];
    if (currVoxelNumLines <= 0) {
        return;
    }

    uint lineListOffset = voxelLineListOffsets[voxelIndex1D];
    for (uint i = 0; i < currVoxelNumLines && i < MAX_NUM_LINES_PER_VOXEL; i++) {
#ifdef PACK_LINES
        decompressLine(lineSegments[lineListOffset+i], currVoxelLines[i]);
#else
        currVoxelLines[i] = lineSegments[lineListOffset+i];
        //currVoxelLines[i].v1 = vec3(voxelIndex);
        //currVoxelLines[i].v2 = vec3(voxelIndex) + vec3(1.0);
#endif
    }
}

// Get density at specified lod index
float getVoxelDensity(vec3 coords, float lod)
{
    return textureLod(densityTexture, coords / vec3(gridResolution), lod).r;
}



#endif