//
// Created by christoph on 02.10.18.
//

#ifndef PIXELSYNCOIT_VOXELCURVEDISCRETIZER_HPP
#define PIXELSYNCOIT_VOXELCURVEDISCRETIZER_HPP

#include <vector>
#include <list>
#include <set>

#include <glm/glm.hpp>

#include <Math/Geometry/Ray3.hpp>
#include <Math/Geometry/Plane.hpp>
#include <Math/Geometry/AABB3.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Color.hpp>

#include "Utils/ImportanceCriteria.hpp"
#include "VoxelData.hpp"

struct AttributePoint
{
    AttributePoint(const glm::vec3 &v, float a) : v(v), a(a) {}
    glm::vec3 v;
    float a;
};

class VoxelDiscretizer
{
public:
    // Returns true if the passed line intersects the voxel boundaries
    bool addPossibleIntersections(const glm::vec3 &v1, const glm::vec3 &v2, float a1, float a2);
    void setIndex(glm::ivec3 index);
    const glm::ivec3 &getIndex() const { return index; }
    float computeDensity(float maxVorticity);
    float computeDensityHair(float opacity);

    glm::ivec3 index;
    std::vector<LineSegment> lines;

    // For clipping lines to voxel
    std::vector<AttributePoint> currentCurveIntersections;
};



class VoxelCurveDiscretizer
{
public:
    VoxelCurveDiscretizer(
            const glm::ivec3 &gridResolution = glm::ivec3(256, 256, 256),
            const glm::ivec3 &quantizationResolution = glm::ivec3(8, 8, 8));
    ~VoxelCurveDiscretizer();
    VoxelGridDataCompressed createFromTrajectoryDataset(const std::string &filename, TrajectoryType trajectoryType,
            std::vector<float> &attributes, float &maxVorticity, unsigned int maxNumLinesPerVoxel, bool useGPU = true);
    VoxelGridDataCompressed createFromHairDataset(const std::string &filename, float &lineRadius,
            glm::vec4 &hairStrandColor, unsigned int maxNumLinesPerVoxel, bool useGPU = true);
    glm::mat4 getWorldToVoxelGridMatrix() { return linesToVoxel; }

    // Recompute density and AO factor if the transfer function changed.
    void recreateDensityAndAOFactors(VoxelGridDataCompressed &dataCompressed, VoxelGridDataGPU &dataGPU,
            unsigned int maxNumLinesPerVoxel);

private:
    bool isHairDataset = false;
    glm::ivec3 gridResolution, quantizationResolution;
    VoxelDiscretizer *voxels;

    // Trajectory dataset
    float maxVorticity;
    std::vector<float> attributes;

    // Hair dataset
    glm::vec4 hairStrandColor;
    float hairThickness;
    float hairOpacity;

    // Grid generation
    void setVoxelGrid(const sgl::AABB3 &aabb);

    // On CPU
    VoxelGridDataCompressed compressData();
    void nextStreamline(const Curve &line);
    // On GPU
    VoxelGridDataCompressed createVoxelGridGPU(std::vector<Curve> &curves, unsigned int maxNumLinesPerVoxel);

    // Compression
    void quantizeLine(const glm::vec3 &voxelPos, const LineSegment &line, LineSegmentQuantized &lineQuantized,
            int faceIndex1, int faceIndex2);
    void compressLine(const glm::ivec3 &voxelIndex, const LineSegment &line, LineSegmentCompressed &lineCompressed);
    void quantizePoint(const glm::vec3 &v, glm::ivec2 &qv, int faceIndex);
    int computeFaceIndex(const glm::vec3 &v, const glm::ivec3 &voxelIndex);

    // Test decompression
    glm::vec3 getQuantizedPositionOffset(uint32_t faceIndex, uint32_t quantizedPos1D);
    void decompressLine(const glm::vec3 &voxelPosition, const LineSegmentCompressed &compressedLine,
            LineSegment &decompressedLine);
    bool checkLinesEqual(const LineSegment &originalLine, const LineSegment &decompressedLine);

    std::vector<VoxelDiscretizer*> getVoxelsInAABB(const sgl::AABB3 &aabb);
    sgl::AABB3 linesBoundingBox;
    glm::mat4 linesToVoxel, voxelToLines;
};

std::string ivec3ToString(const glm::ivec3 &v);

#endif //PIXELSYNCOIT_VOXELCURVEDISCRETIZER_HPP
