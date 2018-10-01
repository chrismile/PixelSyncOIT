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
/*
struct Line
{
    std::vector<glm::vec3> points;
    std::vector<float> attributes;
};

class LineSegment
{
    glm::vec3 v1, v2;
    float a1, a2;
};

class Voxel
{
public:
    void intersect(const glm::vec3 &v1, const glm::vec3 &v2, float a1, float a2);

    glm::ivec3 index;
    float density;
    std::vector<LineSegment> lines;

    // For clipping line to voxel
    std::list<glm::vec3> currentLineIntersections;
};

bool Voxel::intersect(const glm::vec3 &v1, const glm::vec3 &v2, float a1, float a2)
{
    if (true) {
        // TODO
    }
    return true;
}


class VoxelCurveDiscretizer
{
public:
    VoxelCurveDiscretizer(
            const glm::ivec3 &gridResolution = glm::ivec3(256, 256, 256),
            const glm::ivec3 &quantizationResolution = glm::ivec3(8, 8, 8));
    void saveToFile(const std::string &filename);
    void loadFromFile(const std::string &filename);
    void setLines(const std::vector<Line> &lines);

private:
    glm::ivec3 gridResolution, quantizationResolution;
    Voxel *voxels;

    void insertLine(const Line &line);
    void quantizeLine(const glm::vec3 &v1, const glm::vec3 &v2, float attr1, float attr2);
    sgl::AABB3 linesBoundingBox;
    glm::mat4 linesToVoxel, voxelToLines;
};

void VoxelCurveDiscretizer::setLines(const std::vector<Line> &lines)
{
    linesBoundingBox = sgl::AABB3();
    for (const Line &line : lines) {
        for (const glm::vec3 &v : line.points) {
            linesBoundingBox.combine(v);
        }
    }

    // Move to origin and scale to range from (0, 0, 0) to (rx, ry, rz).
    linesToVoxel = sgl::matrixTranslation(-linesBoundingBox.getMinimum());
    linesToVoxel = sgl::matrixScaling(1.0 / linesBoundingBox.getExtent() * gridResolution) * linesToVoxel;
    voxelToLines = glm::inverse(linesToVoxel);

    // Insert lines into voxel representation
    for (const Line &line : lines) {
        insertLine(line);
    }
}

void VoxelCurveDiscretizer::insertLine(const Line &line)
{
    int N = line.points.size();

    // Add intersections to voxels
    std::set<Voxel*> usedVoxels;
    for (int i = 0; i < N-1; i++) {
        // Get line segment
        glm::vec3 v1 = line.points.at(i);
        glm::vec3 v2 = line.points.at(i+1);
        float a1 = line.attributes.at(i);
        float a2 = line.attributes.at(i+1);

        // Compute AABB of current segment
        sgl::AABB3 segmentAABB = sgl::AABB3();
        segmentAABB.combine(v1);
        segmentAABB.combine(v2);

        // Iterate over all voxels with possible intersections
        std::vector<Voxel*> voxelsInAABB = getVoxelsInAABB(segmentAABB);

        for (Voxel *voxel : voxelsInAABB) {
            // Line-voxel intersection
            if (voxel->intersect(v1, v2, a1, a2)) {
                // Intersection(s) added to "currentLineIntersections", voxel used
                usedVoxels.insert(voxel);
            }
        }
    }

    // Convert intersections to clipped line segments
    for (Voxel *voxel : usedVoxels) {
        auto it1 = voxel->currentLineIntersections.begin();
        auto it2 = voxel->currentLineIntersections.begin()++;
        while (it2 != voxel->currentLineIntersections.end()) {
            voxel->lines.push_back(LineSegment(it1->v, it1->a, it2->v, it2->a));
            it1++; it1++;
            if (it1 == voxel->currentLineIntersections.end()) break;
            it2++; it2++;
        }
    }
}

template<typename T>
T clamp(T x, T a, T b) {
    if (x < a) {
        return a;
    } else if (x > b) {
        return b;
    } else {
        return x;
    }
}

void VoxelCurveDiscretizer::quantizePoint(const glm::vec3 &v, glm::ivec3 &qv)
{
    // Iterate over all dimensions
    for (int i = 0; i < 3; i++) {
        int quantizationPos = std::lround(v[i] * quantizationResolution + 0.5);
        qv[i] = clamp(quantizationPos, 0, quantizationResolution-1);
    }
}


class VoxelGridGPU
{
public:
    void uploadVoxelDensities();

private:
    glm::ivec3 gridResolution, quantizationResolution;

    std::vector<LineSegmentCompressed> lineList;
    std::vector<uint32_t> voxelLineListOffsets;
    std::vector<float> voxelDensities;
};

void VoxelGridGPU::uploadVoxelDensities()
{
    ;

    // Compute LODs

}
*/

#endif //PIXELSYNCOIT_VOXELCURVEDISCRETIZER_HPP
