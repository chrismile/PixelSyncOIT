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
    float computeDensity(float maxVorticity);

    glm::ivec3 index;
    std::vector<LineSegment> lines;

    // For clipping lines to voxel
    std::list<AttributePoint> currentCurveIntersections;
};



class VoxelCurveDiscretizer
{
public:
    VoxelCurveDiscretizer(
            const glm::ivec3 &gridResolution = glm::ivec3(256, 256, 256),
            const glm::ivec3 &quantizationResolution = glm::ivec3(8, 8, 8));
    ~VoxelCurveDiscretizer();
    void createFromFile(const std::string &filename);
    VoxelGridDataCompressed compressData();
    glm::mat4 getWorldToVoxelGridMatrix() { return linesToVoxel; }

private:
    glm::ivec3 gridResolution, quantizationResolution;
    VoxelDiscretizer *voxels;
    float maxVorticity;

    void nextStreamline(const Curve &line);
    void insertLine(const Curve &line);

    void quantizeLine(const LineSegment &line, LineSegmentQuantized &lineQuantized, int faceIndex1, int faceIndex2);
    void compressLine(const LineSegment &line, LineSegmentCompressed &lineCompressed, const glm::ivec3 &voxelIndex);
    void quantizePoint(const glm::vec3 &v, glm::ivec2 &qv, int faceIndex1, int faceIndex2);
    int computeFaceIndex(const glm::vec3 &v, const glm::ivec3 &voxelIndex);

    std::vector<VoxelDiscretizer*> getVoxelsInAABB(const sgl::AABB3 &aabb);
    sgl::AABB3 linesBoundingBox;
    glm::mat4 linesToVoxel, voxelToLines;
};



#endif //PIXELSYNCOIT_VOXELCURVEDISCRETIZER_HPP
