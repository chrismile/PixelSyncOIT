//
// Created by christoph on 02.10.18.
//

#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <Utils/Convert.hpp>
#include <Utils/File/Logfile.hpp>

#include "VoxelCurveDiscretizer.hpp"

#define BIAS 0.001

/**
 * Helper function for rayBoxIntersection (see below).
 */
bool rayBoxPlaneIntersection(float rayOriginX, float rayDirectionX, float lowerX, float upperX,
                             float &tNear, float &tFar)
{
    if (abs(rayDirectionX) < BIAS) {
        // Ray is parallel to the x planes
        if (rayOriginX < lowerX || rayOriginX > upperX) {
            return false;
        }
    } else {
        // Not parallel to the x planes. Compute the intersection distance to the planes.
        float t0 = (lowerX - rayOriginX) / rayDirectionX;
        float t1 = (upperX - rayOriginX) / rayDirectionX;
        if (t0 > t1) {
            // Since t0 intersection with near plane
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }

        if (t0 > tNear) {
            // We want the largest tNear
            tNear = t0;
        }
        if (t1 < tFar) {
            // We want the smallest tFar
            tFar = t1;
        }
        if (tNear > tFar) {
            // Box is missed
            return false;
        }
        if (tFar < 0) {
            // Box is behind ray
            return false;
        }
    }
    return true;
}

/**
 * Implementation of ray-box intersection (idea from A. Glassner et al., "An Introduction to Ray Tracing").
 * For more details see: https://www.siggraph.org//education/materials/HyperGraph/raytrace/rtinter3.htm
 */
bool rayBoxIntersection(glm::vec3 rayOrigin, glm::vec3 rayDirection, glm::vec3 lower, glm::vec3 upper,
                        float &tNear, float &tFar)
{
    tNear = -1e7;
    tFar = 1e7;
    for (int i = 0; i < 3; i++) {
        if (!rayBoxPlaneIntersection(rayOrigin[i], rayDirection[i], lower[i], upper[i], tNear, tFar)) {
            return false;
        }
    }

    //entrancePoint = rayOrigin + tNear * rayDirection;
    //exitPoint = rayOrigin + tFar * rayDirection;
    return true;
}




bool VoxelDiscretizer::addPossibleIntersections(const glm::vec3 &v1, const glm::vec3 &v2, float a1, float a2)
{
    float tNear, tFar;
    glm::vec3 voxelLower = glm::vec3(index);
    glm::vec3 voxelUpper = glm::vec3(index + glm::ivec3(1,1,1));
    if (rayBoxIntersection(v1, (v2 - v1), voxelLower, voxelUpper, tNear, tFar)) {
        bool intersectionNear = 0.0f <= tNear && tNear <= 1.0f;
        bool intersectionFar = 0.0f <= tFar && tFar <= 1.0f;
        if (intersectionNear) {
            glm::vec3 entrancePoint = v1 + tNear * (v2 - v1);
            float interpolatedAttribute = a1 + tNear * (a2 - a1);
            currentCurveIntersections.push_back(AttributePoint(entrancePoint, interpolatedAttribute));
        }
        if (intersectionFar) {
            glm::vec3 exitPoint = v1 + tFar * (v2 - v1);
            float interpolatedAttribute = a1 + tFar * (a2 - a1);
            currentCurveIntersections.push_back(AttributePoint(exitPoint, interpolatedAttribute));
        }
        if (intersectionNear || intersectionFar) {
            return true; // Intersection found
        }
    }
    return false;
}

void VoxelDiscretizer::setIndex(glm::ivec3 index)
{
    this->index = index;
}

float VoxelDiscretizer::computeDensity()
{
    float density = 0.0f;
    for (LineSegment &line : lines) {
        density += line.length() * line.avgOpacity();
    }
    return density;
}





VoxelCurveDiscretizer::VoxelCurveDiscretizer(const glm::ivec3 &gridResolution, const glm::ivec3 &quantizationResolution)
        : gridResolution(gridResolution), quantizationResolution(quantizationResolution)
{
    voxels = new VoxelDiscretizer[gridResolution.x * gridResolution.y * gridResolution.z];
    for (int z = 0; z < gridResolution.z; z++) {
        for (int y = 0; y < gridResolution.y; y++) {
            for (int x = 0; x < gridResolution.x; x++) {
                int index = x + y*gridResolution.x + z*gridResolution.x*gridResolution.y;
                voxels[index].setIndex(glm::ivec3(x, y, z));
            }
        }
    }
}

VoxelCurveDiscretizer::~VoxelCurveDiscretizer()
{
    delete[] voxels;
}

void VoxelCurveDiscretizer::createFromFile(const std::string &filename)
{
    std::ifstream file(filename.c_str());

    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in VoxelCurveDiscretizer::createFromFile: File \""
                                        + filename + "\" does not exist.");
        return;
    }

    linesBoundingBox = sgl::AABB3();
    std::vector<Curve> curves;
    Curve currentCurve;

    std::string lineString;
    while (getline(file, lineString)) {
        while (lineString.size() > 0 && (lineString[lineString.size()-1] == '\r' || lineString[lineString.size()-1] == ' ')) {
            // Remove '\r' of Windows line ending
            lineString = lineString.substr(0, lineString.size() - 1);
        }
        std::vector<std::string> line;
        boost::algorithm::split(line, lineString, boost::is_any_of("\t "), boost::token_compress_on);

        std::string command = line.at(0);

        if (command == "g") {
            // New path
            static int ctr = 0;
            if (ctr >= 999) {
                sgl::Logfile::get()->writeInfo(std::string() + "Parsing trajectory line group " + line.at(1) + "...");
            }
            ctr = (ctr + 1) % 1000;
        } else if (command == "v") {
            // Path line vertex position
            currentCurve.points.push_back(glm::vec3(sgl::fromString<float>(line.at(1)),
                    sgl::fromString<float>(line.at(2)), sgl::fromString<float>(line.at(3))));
            linesBoundingBox.combine(currentCurve.points.back());
        } else if (command == "vt") {
            // Path line vertex attribute
            currentCurve.attributes.push_back(sgl::fromString<float>(line.at(1)));
        } else if (command == "l") {
            // Indices of path line signal all points read of current curve
            curves.push_back(currentCurve);
            currentCurve = Curve();
        } else if (boost::starts_with(command, "#") || command == "") {
            // Ignore comments and empty lines
        } else {
            sgl::Logfile::get()->writeError(std::string() + "Error in parseObjMesh: Unknown command \"" + command + "\".");
        }
    }

    file.close();


    // Move to origin and scale to range from (0, 0, 0) to (rx, ry, rz).
    linesToVoxel = sgl::matrixScaling(1.0f / linesBoundingBox.getDimensions() * glm::vec3(gridResolution))
            * sgl::matrixTranslation(-linesBoundingBox.getMinimum());
    voxelToLines = glm::inverse(linesToVoxel);

    // Transform curves to voxel grid space
    for (Curve &curve : curves) {
        for (glm::vec3 &v : curve.points) {
            v = sgl::transformPoint(linesToVoxel, v);
        }
    }

    // Insert lines into voxel representation
    for (const Curve &curve : curves) {
        nextStreamline(curve);
    }
}



VoxelGridDataCompressed VoxelCurveDiscretizer::compressData()
{
    VoxelGridDataCompressed dataCompressed;
    dataCompressed.gridResolution = gridResolution;
    dataCompressed.quantizationResolution = quantizationResolution;
    dataCompressed.worldToVoxelGridMatrix = this->getWorldToVoxelGridMatrix();

    int n = gridResolution.x * gridResolution.y * gridResolution.z;
    std::vector<float> voxelDensities;
    voxelDensities.reserve(n);

    int lineOffset = 0;
    dataCompressed.voxelLineListOffsets.reserve(n);
    dataCompressed.numLinesInVoxel.reserve(n);
    dataCompressed.lineSegments.reserve(n);

    for (int i = 0; i < n; i++) {
        dataCompressed.voxelLineListOffsets.push_back(lineOffset);
        int numLines = voxels[i].lines.size();
        dataCompressed.numLinesInVoxel.push_back(numLines);
        voxelDensities.push_back(voxels[i].computeDensity());

        // TODO: Compress
        dataCompressed.lineSegments = voxels[i].lines;

        lineOffset += numLines;
    }

    dataCompressed.voxelDensityLODs = generateMipmapsForDensity(&voxelDensities.front(), gridResolution);
    return dataCompressed;
}



std::vector<VoxelDiscretizer*> VoxelCurveDiscretizer::getVoxelsInAABB(const sgl::AABB3 &aabb)
{
    std::vector<VoxelDiscretizer*> voxelsInAABB;
    glm::vec3 minimum = aabb.getMinimum();
    glm::vec3 maximum = aabb.getMaximum();

    glm::ivec3 lower = glm::ivec3(aabb.getMinimum()); // Round down
    glm::ivec3 upper = glm::ivec3(ceil(maximum.x), ceil(maximum.y), ceil(maximum.z)); // Round up
    lower = glm::max(lower, glm::ivec3(0));
    upper = glm::min(upper, gridResolution - glm::ivec3(1));

    for (int z = lower.z; z <= upper.z; z++) {
        for (int y = lower.y; y <= upper.y; y++) {
            for (int x = lower.x; x <= upper.x; x++) {
                int index = x + y*gridResolution.x + z*gridResolution.x*gridResolution.y;
                voxelsInAABB.push_back(voxels + index);
            }
        }
    }
    return voxelsInAABB;
}


void VoxelCurveDiscretizer::nextStreamline(const Curve &line)
{
    int N = line.points.size();

    // Add intersections to voxels
    std::set<VoxelDiscretizer*> usedVoxels;
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
        std::vector<VoxelDiscretizer*> voxelsInAABB = getVoxelsInAABB(segmentAABB);

        for (VoxelDiscretizer *voxel : voxelsInAABB) {
            // Line-voxel intersection
            if (voxel->addPossibleIntersections(v1, v2, a1, a2)) {
                // Intersection(s) added to "currentLineIntersections", voxel used
                usedVoxels.insert(voxel);
            }
        }
    }

    std::cout << "Number of used voxels: " << usedVoxels.size() << std::endl;

    // Convert intersections to clipped line segments
    int ctr = 0;
    for (VoxelDiscretizer *voxel : usedVoxels) {
        if (voxel->currentCurveIntersections.size() < 2) {
            continue;
        }
        auto it1 = voxel->currentCurveIntersections.begin();
        auto it2 = voxel->currentCurveIntersections.begin()++;
        while (it2 != voxel->currentCurveIntersections.end()) {
            voxel->lines.push_back(LineSegment(it1->v, it1->a, it2->v, it2->a));
            it1++; it1++;
            if (it1 == voxel->currentCurveIntersections.end()) break;
            it2++; it2++;
        }
        ctr++;
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
        int quantizationPos = std::lround(v[i] * quantizationResolution[i] + 0.5);
        qv[i] = glm::clamp(quantizationPos, 0, quantizationResolution[i]-1);
    }
}

