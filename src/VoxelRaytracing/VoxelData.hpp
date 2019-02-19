//
// Created by christoph on 04.10.18.
//

#ifndef PIXELSYNCOIT_VOXELDATA_HPP
#define PIXELSYNCOIT_VOXELDATA_HPP

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <Graphics/Buffers/GeometryBuffer.hpp>
#include <Graphics/Texture/Texture.hpp>

#define PACK_LINES

float opacityMapping(float attr, float maxAttr);

struct Curve
{
    std::vector<glm::vec3> points;
    std::vector<float> attributes;
    unsigned int lineID = 0;
};

struct LineSegment
{
    LineSegment(const glm::vec3 &v1, float a1, const glm::vec3 &v2, float a2, unsigned int lineID)
            : v1(v1), a1(a1), v2(v2), a2(a2), lineID(lineID) {}
    LineSegment() : v1(0.0f), a1(0.0f), v2(0.0f), a2(0.0f), lineID(0) {}
    float length() { return glm::length(v2 - v1); }
    float avgOpacity(float maxVorticity) { return (opacityMapping(a1, maxVorticity) + opacityMapping(a2, maxVorticity)) / 2.0f; }

    glm::vec3 v1; // Vertex position
    float a1; // Vertex attribute
    glm::vec3 v2; // Vertex position
    float a2; // Vertex attribute
    unsigned int lineID;
};

struct LineSegmentQuantized
{
    uint8_t faceIndex1; // 3 bits
    uint8_t faceIndex2; // 3 bits
    uint32_t facePositionQuantized1; // 2*log2(QUANTIZATION_RESOLUTION) bits
    uint32_t facePositionQuantized2; // 2*log2(QUANTIZATION_RESOLUTION) bits
    unsigned int lineID; // 8 bits
    float a1; // Attribute 1
    float a2; // Attribute 2
};

struct LineSegmentCompressed
{
    // Bit 0-2, 3-5: Face ID of start/end point.
    // For c = log2(QUANTIZATION_RESOLUTION^2) = 2*log2(QUANTIZATION_RESOLUTION):
    // Bit 6-(5+c), (6+c)-(5+2c): Quantized face position of start/end point.
    uint32_t linePosition;
    // Bit 11-15: Line ID (5 bits for bitmask of 2^5 bits = 32 bits).
    // Bit 16-23, 24-31: Attribute of start/end point (normalized to [0,1]).
    uint32_t attributes;
};


/*class Voxel
{
    std::vector<LineSegment> lineSegments;
};

struct VoxelGridData
{
    glm::ivec3 gridResolution, quantizationResolution;
    std::vector<Voxel> voxels;

    /// Every entry is one LOD
    std::vector<std::vector<float>> voxelDensityLODs;
};*/

struct VoxelGridDataCompressed
{
    glm::ivec3 gridResolution, quantizationResolution;
    glm::mat4 worldToVoxelGridMatrix;
    uint32_t dataType; // 0 for trajectory data, 1 for hair data

    // In case of trajectory data
    float maxVorticity;
    std::vector<float> attributes; // For histogram

    // In case of hair data
    glm::vec4 hairStrandColor;
    float hairThickness;

    std::vector<uint32_t> voxelLineListOffsets;
    std::vector<uint32_t> numLinesInVoxel;

    std::vector<float> voxelDensityLODs;
    std::vector<float> voxelAOLODs;
    std::vector<uint32_t> octreeLODs;

#ifdef PACK_LINES
    std::vector<LineSegmentCompressed> lineSegments;
#else
    std::vector<LineSegment> lineSegments;
#endif
};

struct VoxelGridDataGPU
{
    glm::ivec3 gridResolution, quantizationResolution;
    glm::mat4 worldToVoxelGridMatrix;

    sgl::GeometryBufferPtr voxelLineListOffsets;
    sgl::GeometryBufferPtr numLinesInVoxel;

    sgl::TexturePtr densityTexture;
    sgl::TexturePtr aoTexture;
    sgl::TexturePtr octreeTexture;

    sgl::GeometryBufferPtr lineSegments;
};


void saveToFile(const std::string &filename, const VoxelGridDataCompressed &data);
void loadFromFile(const std::string &filename, VoxelGridDataCompressed &data);
void compressedToGPUData(const VoxelGridDataCompressed &compressedData, VoxelGridDataGPU &gpuData);
std::vector<float> generateMipmapsForDensity(float *density, glm::ivec3 size);
std::vector<uint32_t> generateMipmapsForOctree(uint32_t *numLines, glm::ivec3 size);
sgl::TexturePtr generateDensityTexture(const std::vector<float> &lods, glm::ivec3 size);
void generateVoxelAOFactorsFromDensity(const std::vector<float> &voxelDensities, std::vector<float> &voxelAOFactors,
                                       glm::ivec3 size, bool isHairDataset);

#endif //PIXELSYNCOIT_VOXELDATA_HPP
