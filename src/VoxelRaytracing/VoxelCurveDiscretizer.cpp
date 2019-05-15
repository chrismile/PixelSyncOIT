//
// Created by christoph on 02.10.18.
//

#include <fstream>
#include <iostream>
#include <chrono>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <GL/glew.h>

#include <Utils/Convert.hpp>
#include <Utils/File/Logfile.hpp>
#include <Math/Math.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/Texture.hpp>

#include "Utils/HairLoader.hpp"
#include "VoxelCurveDiscretizer.hpp"

#define BIAS 0.001

/**
 * Helper function for rayBoxIntersection (see below).
 */
bool rayBoxPlaneIntersection(float rayOriginX, float rayDirectionX, float lowerX, float upperX,
                             float &tNear, float &tFar)
{
    if (std::abs(rayDirectionX) < BIAS) {
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

float VoxelDiscretizer::computeDensity(float maxVorticity)
{
    float density = 0.0f;
    for (LineSegment &line : lines) {
        density += line.length() * line.avgOpacity(maxVorticity);
    }
    return density;
}

float VoxelDiscretizer::computeDensityHair(float opacity)
{
    float density = 0.0f;
    for (LineSegment &line : lines) {
        density += line.length() * opacity;
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

VoxelGridDataCompressed VoxelCurveDiscretizer::createFromTrajectoryDataset(const std::string &filename, std::vector<float> &attributes,
        float &_maxVorticity, unsigned int maxNumLinesPerVoxel, bool useGPU)
{
    linesBoundingBox = sgl::AABB3();
    std::vector<Curve> curves;
    Curve currentCurve;
    maxVorticity = 0.0f;
    int lineCounter = 0;
    isHairDataset = false;
    uint64_t numLineSegments = 0;
    uint64_t numLines = 0;

    bool isRings = boost::starts_with(filename, "Data/Rings");
    bool isConvectionRolls = boost::starts_with(filename, "Data/ConvectionRolls/output");






    FILE *file = fopen(filename.c_str(), "r");
    if (!file) {
        sgl::Logfile::get()->writeError(std::string() + "Error in VoxelCurveDiscretizer::createFromTrajectoryDataset: "
                "File \"" + filename + "\" does not exist.");
        return VoxelGridDataCompressed();
    }
    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *fileBuffer = new char[length];
    fread(fileBuffer, 1, length, file);
    fclose(file);
    std::string lineBuffer;
    std::string numberString;

    for (size_t charPtr = 0; charPtr < length; charPtr++) {
        while (charPtr < length) {
            char currentChar = fileBuffer[charPtr];
            if (currentChar == '\n' || currentChar == '\r') {
                charPtr++;
                break;
            }
            lineBuffer.push_back(currentChar);
            charPtr++;
        }

        if (lineBuffer.size() == 0) {
            continue;
        }

        char command = lineBuffer.at(0);
        char command2 = ' ';
        if (lineBuffer.size() > 1) {
            command2 = lineBuffer.at(1);
        }

        if (command == 'g') {
            // New path
            /*static int ctr = 0;
            if (ctr >= 999) {
                Logfile::get()->writeInfo(std::string() + "Parsing trajectory line group " + line.at(1) + "...");
            }
            ctr = (ctr + 1) % 1000;*/
            lineCounter++;
        } else if (command == 'v' && command2 == 't') {
            // Path line vertex attribute
            float attr = 0.0f;
            sscanf(lineBuffer.c_str()+2, "%f", &attr);
            currentCurve.attributes.push_back(attr);
            attributes.push_back(attr);
            maxVorticity = std::max(maxVorticity, attr);
        } else if (command == 'v' && command2 == 'n') {
            // Not supported so far
        } else if (command == 'v') {
            // Path line vertex position
            glm::vec3 position;
            if (isConvectionRolls) {
                sscanf(lineBuffer.c_str()+2, "%f %f %f", &position.x, &position.z, &position.y);
            } else {
                sscanf(lineBuffer.c_str()+2, "%f %f %f", &position.x, &position.y, &position.z);
            }

            // Path line vertex position
            currentCurve.points.push_back(position);
            linesBoundingBox.combine(position);
        } else if (command == 'l') {
            // Indices of path line signal all points read of current curve
            numLines++;
            numLineSegments += currentCurve.points.size() - 1;
            curves.push_back(currentCurve);
            currentCurve = Curve();
            currentCurve.lineID = lineCounter;
        } else if (command = '#') {
            // Ignore comments
        } else {
            //Logfile::get()->writeError(std::string() + "Error in VoxelCurveDiscretizer::createFromTrajectoryDataset: "
            //        "Unknown command \"" + command + "\".");
        }

        lineBuffer.clear();
    }

    std::cout << "Num Lines: " << numLines << std::endl;
    std::cout << "Num LineSegments: " << numLineSegments << std::endl << std::flush;


    // Normalize data for rings
    float minValue = std::min(linesBoundingBox.getMinimum().x, std::min(linesBoundingBox.getMinimum().y, linesBoundingBox.getMinimum().z));
    float maxValue = std::max(linesBoundingBox.getMaximum().x, std::max(linesBoundingBox.getMaximum().y, linesBoundingBox.getMaximum().z));

//    sgl::AABB3 linesBoundingBoxNew = line

    if (isConvectionRolls)
    {
        minValue = 0;
        maxValue = 0.5;
    }

    glm::vec3 minVec(minValue, minValue, minValue);
    glm::vec3 maxVec(maxValue, maxValue, maxValue);

    float dimY = linesBoundingBox.getDimensions().y;

    if (isRings || isConvectionRolls)
    {
        linesBoundingBox = sgl::AABB3();

        for (auto& curve : curves)
        {
            for (auto &point : curve.points)
            {
                point = (point - minVec) / (maxVec - minVec);

                if (isConvectionRolls)
                {
                    glm::vec3 dims = glm::vec3(1);
                    dims.y = dimY;
                    point -= dims;
                }

                linesBoundingBox.combine(point);
//
            }
        }
    }

    if (isRings)
    {
        linesBoundingBox = sgl::AABB3();
        linesBoundingBox.combine(glm::vec3(-2));
        linesBoundingBox.combine(glm::vec3(2));
    }

    if (isConvectionRolls)
    {
        linesBoundingBox = sgl::AABB3();
        linesBoundingBox.combine(glm::vec3(-1.5, -0.5, -1.5));
        linesBoundingBox.combine(glm::vec3( 1.5, 0.5, 1.5));
    }

    std::cout << "Bounding box: " << linesBoundingBox.getMaximum().x << " " << linesBoundingBox.getMaximum().y << " " << linesBoundingBox.getMaximum().z << std::endl << std::flush;

    // HACK
    sgl::AABB3 linesBoundingBoxTemp = linesBoundingBox;

    const float minZ = linesBoundingBox.getMinimum().z;
    const float dimZ = linesBoundingBox.getDimensions().z;

//    for (auto& curve : curves)
//    {
//        for (auto& point : curve.points)
//        {
//            point.z = (point.z - minZ) / (dimZ);
//            linesBoundingBoxTemp.combine(point);
//        }
//    }

    _maxVorticity = maxVorticity;
    this->attributes = attributes;


    // Move to origin and scale to range from (0, 0, 0) to (rx, ry, rz).
    linesToVoxel = sgl::matrixScaling(1.0f / linesBoundingBox.getDimensions() * glm::vec3(gridResolution))
            * sgl::matrixTranslation(-linesBoundingBox.getMinimum());

    glm::mat4 linesToVoxelTemp = sgl::matrixScaling(1.0f / linesBoundingBoxTemp.getDimensions() * glm::vec3(gridResolution))
                   * sgl::matrixTranslation(-linesBoundingBoxTemp.getMinimum());

    voxelToLines = glm::inverse(linesToVoxel);

    // Transform curves to voxel grid space
    for (Curve &curve : curves) {
        for (glm::vec3 &v : curve.points) {
            v = sgl::transformPoint(linesToVoxel, v);
        }
    }

    if (!useGPU) {
        // Insert lines into voxel representation
        //int lineNum = 0;
        for (const Curve &curve : curves) {
            /*if (lineNum == 1000) {
                break;
            }*/
            nextStreamline(curve);
            //lineNum++;
        }
        return compressData();
    } else {
        return createVoxelGridGPU(curves, maxNumLinesPerVoxel);
    }
}


VoxelGridDataCompressed VoxelCurveDiscretizer::createFromHairDataset(const std::string &filename, float &lineRadius,
        glm::vec4 &hairStrandColor, unsigned int maxNumLinesPerVoxel, bool useGPU)
{
    HairData hairData;
    loadHairFile(filename, hairData);
    downscaleHairData(hairData, HAIR_MODEL_SCALING_FACTOR);

    // Assume default thickness, opacity and color for now to simplify the implementation
    lineRadius = hairData.defaultThickness;
    hairStrandColor = glm::vec4(hairData.defaultColor, hairData.defaultOpacity);
    this->hairThickness = lineRadius;
    this->hairStrandColor = hairStrandColor;
    this->hairOpacity = hairData.defaultOpacity;


    linesBoundingBox = sgl::AABB3();
    std::vector<Curve> curves;
    Curve currentCurve;
    maxVorticity = 0.0f;
    int lineCounter = 0;
    isHairDataset = true;

    // Process all strands and convert them to curves
    for (HairStrand &strand : hairData.strands) {
        lineCounter++;
        if (lineCounter % 1000 == 999) {
            sgl::Logfile::get()->writeInfo(std::string() + "Parsing hair strand " + sgl::toString(lineCounter) + "...");
        }

        for (glm::vec3 point : strand.points) {
            glm::vec3 scaledPoint = point;
            currentCurve.points.push_back(scaledPoint);
            currentCurve.attributes.push_back(this->hairOpacity);
            linesBoundingBox.combine(scaledPoint);
        }

        curves.push_back(currentCurve);
        currentCurve = Curve();
        currentCurve.lineID = lineCounter;
    }

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

    if (!useGPU) {
        // Insert lines into voxel representation
        for (const Curve &curve : curves) {
            nextStreamline(curve);
        }
        return compressData();
    } else {
        return createVoxelGridGPU(curves, maxNumLinesPerVoxel);
    }
}


VoxelGridDataCompressed VoxelCurveDiscretizer::compressData()
{
    VoxelGridDataCompressed dataCompressed;
    dataCompressed.gridResolution = gridResolution;
    dataCompressed.quantizationResolution = quantizationResolution;
    dataCompressed.worldToVoxelGridMatrix = this->getWorldToVoxelGridMatrix();
    dataCompressed.dataType = isHairDataset ? 1u : 0u;

    if (isHairDataset) {
        dataCompressed.hairStrandColor = hairStrandColor;
        dataCompressed.hairThickness = hairThickness;
    } else {
        dataCompressed.attributes = attributes;
        dataCompressed.maxVorticity = maxVorticity;
    }

    int n = gridResolution.x * gridResolution.y * gridResolution.z;
    std::vector<float> voxelDensities;
    std::vector<uint32_t> usedVoxels;
    voxelDensities.reserve(n);
    usedVoxels.reserve(n);

    size_t lineOffset = 0;
    dataCompressed.voxelLineListOffsets.reserve(n);
    dataCompressed.numLinesInVoxel.reserve(n);
    dataCompressed.lineSegments.clear();

    for (int i = 0; i < n; i++) {
        dataCompressed.voxelLineListOffsets.push_back(lineOffset);
        size_t numLines = voxels[i].lines.size();
        dataCompressed.numLinesInVoxel.push_back(numLines);
        if (isHairDataset) {
            voxelDensities.push_back(voxels[i].computeDensityHair(hairOpacity));
        } else {
            voxelDensities.push_back(voxels[i].computeDensity(maxVorticity));
        }
        usedVoxels.push_back(numLines > 0 ? 1 : 0);

#ifdef PACK_LINES
        std::vector<LineSegmentCompressed> lineSegments;
        lineSegments.resize(voxels[i].lines.size());
        for (size_t j = 0; j < voxels[i].lines.size(); j++) {
            compressLine(voxels[i].getIndex(), voxels[i].lines[j], lineSegments[j]);

            // Test
            /*LineSegment originalLine = voxels[i].lines[j];
            LineSegment decompressedLine;
            decompressLine(glm::vec3(voxels[i].getIndex()), lineSegments[j], decompressedLine);
            if (!checkLinesEqual(originalLine, decompressedLine)) {
                compressLine(voxels[i].getIndex(), voxels[i].lines[j], lineSegments[j]);
                decompressLine(glm::vec3(voxels[i].getIndex()), lineSegments[j], decompressedLine);
            }*/
        }
        dataCompressed.lineSegments.insert(dataCompressed.lineSegments.end(),
                lineSegments.begin(), lineSegments.end());
#else
        dataCompressed.lineSegments.insert(dataCompressed.lineSegments.end(),
                voxels[i].lines.begin(), voxels[i].lines.end());
#endif

        lineOffset += numLines;
    }

    std::vector<float> voxelAOFactors;
    voxelAOFactors.resize(n);
    generateVoxelAOFactorsFromDensity(voxelDensities, voxelAOFactors, gridResolution, isHairDataset);

    dataCompressed.voxelDensityLODs = generateMipmapsForDensity(&voxelDensities.front(), gridResolution);
    dataCompressed.voxelAOLODs = generateMipmapsForDensity(&voxelAOFactors.front(), gridResolution);
    dataCompressed.octreeLODs = generateMipmapsForOctree(&usedVoxels.front(), gridResolution);
    return dataCompressed;
}



std::vector<VoxelDiscretizer*> VoxelCurveDiscretizer::getVoxelsInAABB(const sgl::AABB3 &aabb)
{
    std::vector<VoxelDiscretizer*> voxelsInAABB;
    glm::vec3 minimum = aabb.getMinimum();
    glm::vec3 maximum = aabb.getMaximum();

    glm::ivec3 lower = glm::ivec3(minimum); // Round down
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

        // Remove invalid line points (used in many scientific datasets to indicate invalid lines).
        const float MAX_VAL = 1e10;
        if (std::fabs(v1.x) > MAX_VAL || std::fabs(v1.y) > MAX_VAL || std::fabs(v1.z) > MAX_VAL
                || std::fabs(v2.x) > MAX_VAL || std::fabs(v2.y) > MAX_VAL || std::fabs(v2.z) > MAX_VAL) {
            continue;
        }

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

    // Convert intersections to clipped line segments
    for (VoxelDiscretizer *voxel : usedVoxels) {
        if (voxel->currentCurveIntersections.size() < 2) {
            voxel->currentCurveIntersections.clear();
            continue;
        }
        auto it1 = voxel->currentCurveIntersections.begin();
        auto it2 = voxel->currentCurveIntersections.begin();
        it2++;
        while (it2 != voxel->currentCurveIntersections.end()) {
            voxel->lines.push_back(LineSegment(it1->v, it1->a, it2->v, it2->a, line.lineID));
            it1++; it1++;
            if (it1 == voxel->currentCurveIntersections.end()) break;
            it2++; it2++;
        }
        voxel->currentCurveIntersections.clear();
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

void VoxelCurveDiscretizer::quantizeLine(const glm::vec3 &voxelPos, const LineSegment &line,
        LineSegmentQuantized &lineQuantized, int faceIndex1, int faceIndex2)
{
    lineQuantized.a1 = line.a1;
    lineQuantized.a2 = line.a2;

    glm::ivec2 facePosition3D1, facePosition3D2;
    quantizePoint(line.v1 - voxelPos, facePosition3D1, faceIndex1);
    quantizePoint(line.v2 - voxelPos, facePosition3D2, faceIndex2);
    lineQuantized.lineID = line.lineID;
    lineQuantized.faceIndex1 = faceIndex1;
    lineQuantized.faceIndex2 = faceIndex2;
    lineQuantized.facePositionQuantized1 = facePosition3D1.x + facePosition3D1.y*quantizationResolution.x;
    lineQuantized.facePositionQuantized2 = facePosition3D2.x + facePosition3D2.y*quantizationResolution.x;
}

int intlog2(int x) {
    int exponent = 0;
    while (x > 1) {
        x /= 2;
        exponent++;
    }
    return exponent;
}

void VoxelCurveDiscretizer::compressLine(const glm::ivec3 &voxelIndex, const LineSegment &line,
        LineSegmentCompressed &lineCompressed)
{
    LineSegmentQuantized lineQuantized;
    int faceIndex1 = computeFaceIndex(line.v1, voxelIndex);
    int faceIndex2 = computeFaceIndex(line.v2, voxelIndex);
    quantizeLine(glm::vec3(voxelIndex), line, lineQuantized, faceIndex1, faceIndex2);

    uint8_t attr1Unorm = std::round(lineQuantized.a1*255.0f);
    uint8_t attr2Unorm = std::round(lineQuantized.a2*255.0f);

    int c = round(2*intlog2(quantizationResolution.x));
    lineCompressed.linePosition = lineQuantized.faceIndex1;
    lineCompressed.linePosition |= lineQuantized.faceIndex2 << 3;
    lineCompressed.linePosition |= lineQuantized.facePositionQuantized1 << 6;
    lineCompressed.linePosition |= lineQuantized.facePositionQuantized2 << (6 + c);
    lineCompressed.attributes = 0;
    lineCompressed.attributes |= (lineQuantized.lineID & 31u) << 11;
    lineCompressed.attributes |= attr1Unorm << 16;
    lineCompressed.attributes |= attr2Unorm << 24;
}

void VoxelCurveDiscretizer::quantizePoint(const glm::vec3 &v, glm::ivec2 &qv, int faceIndex)
{
    int dimensions[2];
    if (faceIndex == 0 || faceIndex == 1) {
        // x face
        dimensions[0] = 1;
        dimensions[1] = 2;
    } else if (faceIndex == 2 || faceIndex == 3) {
        // y face
        dimensions[0] = 0;
        dimensions[1] = 2;
    } else {
        // z face
        dimensions[0] = 0;
        dimensions[1] = 1;
    }

    // Iterate over all dimensions
    for (int i = 0; i < 2; i++) {
        int quantizationPos = std::floor(v[dimensions[i]] * quantizationResolution[dimensions[i]]);
        qv[i] = glm::clamp(quantizationPos, 0, quantizationResolution[dimensions[i]]-1);
    }
}

int VoxelCurveDiscretizer::computeFaceIndex(const glm::vec3 &v, const glm::ivec3 &voxelIndex)
{
    glm::ivec3 lower = voxelIndex, upper = voxelIndex + glm::ivec3(1);
    for (int i = 0; i < 3; i++) {
        if (std::abs(v[i] - lower[i]) < 0.00001f) {
            return 2*i;
        }
        if (std::abs(v[i] - upper[i]) < 0.00001f) {
            return 2*i+1;
        }
    }
    sgl::Logfile::get()->writeError(std::string() + "Error in VoxelCurveDiscretizer::computeFaceIndex: "
            + "Invalid position.");
    return 0;
}




glm::vec3 VoxelCurveDiscretizer::getQuantizedPositionOffset(uint32_t faceIndex, uint32_t quantizedPos1D)
{
    glm::vec2 quantizedFacePosition = glm::vec2(
            float(quantizedPos1D % quantizationResolution.x),
            float(quantizedPos1D / quantizationResolution.x))
                    / float(quantizationResolution.x);

    // Whether the face is the face in x/y/z direction with greater dimensions (offset factor)
    float face0or1 = float(faceIndex % 2);

    glm::vec3 offset;
    if (faceIndex <= 1) {
        offset = glm::vec3(face0or1, quantizedFacePosition.x, quantizedFacePosition.y);
    } else if (faceIndex <= 3) {
        offset = glm::vec3(quantizedFacePosition.x, face0or1, quantizedFacePosition.y);
    } else if (faceIndex <= 5) {
        offset = glm::vec3(quantizedFacePosition.x, quantizedFacePosition.y, face0or1);
    }
    return offset;
}


void VoxelCurveDiscretizer::decompressLine(const glm::vec3 &voxelPosition, const LineSegmentCompressed &compressedLine,
        LineSegment &decompressedLine)
{
    const uint32_t c = 2*log2(quantizationResolution.x);
    const uint32_t bitmaskQuantizedPos = quantizationResolution.x*quantizationResolution.x-1;
    uint32_t faceStartIndex = compressedLine.linePosition & 0x7u;
    uint32_t faceEndIndex = (compressedLine.linePosition >> 3) & 0x7u;
    uint32_t quantizedStartPos1D = (compressedLine.linePosition >> 6) & bitmaskQuantizedPos;
    uint32_t quantizedEndPos1D = (compressedLine.linePosition >> 6+c) & bitmaskQuantizedPos;
    uint32_t lineID = (compressedLine.attributes >> 11) & 32u;
    uint32_t attr1 = (compressedLine.attributes >> 16) & 0xFFu;
    uint32_t attr2 = (compressedLine.attributes >> 24) & 0xFFu;

    decompressedLine.v1 = voxelPosition + getQuantizedPositionOffset(faceStartIndex, quantizedStartPos1D);
    decompressedLine.v2 = voxelPosition + getQuantizedPositionOffset(faceEndIndex, quantizedEndPos1D);
    decompressedLine.a1 = float(attr1) / 255.0f;
    decompressedLine.a2 = float(attr2) / 255.0f;
    decompressedLine.lineID = lineID;
}

bool VoxelCurveDiscretizer::checkLinesEqual(const LineSegment &originalLine, const LineSegment &decompressedLine)
{
    bool linesEqual = true;
    if (originalLine.lineID % 256 != decompressedLine.lineID) {
        linesEqual = false;
        sgl::Logfile::get()->writeError("VoxelCurveDiscretizer::checkLinesEqual: lineID");
    }

    if (std::abs(originalLine.a1 - decompressedLine.a1) > 0.01f
            || std::abs(originalLine.a2 - decompressedLine.a2) > 0.01f) {
        linesEqual = false;
        sgl::Logfile::get()->writeError("VoxelCurveDiscretizer::checkLinesEqual: attribute");
    }

    if (glm::length(originalLine.v1 - decompressedLine.v1) > 0.5f) {
        linesEqual = false;
        sgl::Logfile::get()->writeError(std::string() + "VoxelCurveDiscretizer::checkLinesEqual: position2, error: "
                + sgl::toString(glm::length(originalLine.v2 - decompressedLine.v2)));
    }

    if (glm::length(originalLine.v2 - decompressedLine.v2) > 0.5f) {
        linesEqual = false;
        sgl::Logfile::get()->writeError(std::string() + "VoxelCurveDiscretizer::checkLinesEqual: position1, error: "
                                        + sgl::toString(glm::length(originalLine.v2 - decompressedLine.v2)));
    }

    return linesEqual;
}


struct LinePoint {
    LinePoint(glm::vec3 linePoint, float lineAttribute) : linePoint(linePoint), lineAttribute(lineAttribute) {}
    glm::vec3 linePoint;
    float lineAttribute;
};

std::string ivec3ToString(const glm::ivec3 &v) {
    return std::string() + "ivec3(" + sgl::toString(v.x) + ", " + sgl::toString(v.y) + ", " + sgl::toString(v.z) + ")";
}

VoxelGridDataCompressed VoxelCurveDiscretizer::createVoxelGridGPU(
        std::vector<Curve> &curves, unsigned int maxNumLinesPerVoxel)
{
    glm::ivec3 numWorkGroupsVoxel = glm::ivec3(sgl::iceil(gridResolution.x, 64), sgl::iceil(gridResolution.y, 4),
            gridResolution.z);
    uint32_t gridSize1D = gridResolution.x *gridResolution.y *gridResolution.z;
    uint32_t zeroData = 0u;
    void *bufferMemory;


    // Set preprocessor defines for the shaders.
    sgl::ShaderManager->addPreprocessorDefine("MAX_NUM_LINES_PER_VOXEL", maxNumLinesPerVoxel);
    sgl::ShaderManager->addPreprocessorDefine("gridResolution", ivec3ToString(gridResolution));
    sgl::ShaderManager->addPreprocessorDefine(
            "GRID_RESOLUTION_LOG2", sgl::toString(sgl::intlog2(gridResolution.x)));
    sgl::ShaderManager->addPreprocessorDefine("GRID_RESOLUTION", gridResolution.x);
    sgl::ShaderManager->addPreprocessorDefine("quantizationResolution", ivec3ToString(quantizationResolution));
    sgl::ShaderManager->addPreprocessorDefine("QUANTIZATION_RESOLUTION", sgl::toString(quantizationResolution.x));
    sgl::ShaderManager->addPreprocessorDefine(
            "QUANTIZATION_RESOLUTION_LOG2", sgl::toString(sgl::intlog2(quantizationResolution.x)));


    // PART 1: Create the LinePointBuffer, LineOffsetBuffer, NumSegmentsBuffer (empty) and LineSegmentsBuffer.
    auto startBuffers = std::chrono::system_clock::now();
    std::vector<LinePoint> linePoints;
    std::vector<uint32_t> lineOffsets;
    lineOffsets.push_back(0);
    uint32_t offsetCounter = 0;
    for (Curve &curve : curves) {
        size_t curveNumPoints = curve.points.size();
        for (size_t i = 0; i < curveNumPoints; i++) {
            linePoints.push_back(LinePoint(curve.points.at(i), curve.attributes.at(i)));
        }
        offsetCounter += curveNumPoints;
        lineOffsets.push_back(offsetCounter);
    }
    sgl::GeometryBufferPtr linePointBuffer = sgl::Renderer->createGeometryBuffer(
            linePoints.size()+1 * sizeof(LinePoint), &linePoints.front(),
            sgl::SHADER_STORAGE_BUFFER, sgl::BUFFER_STATIC);
    sgl::GeometryBufferPtr lineOffsetBuffer = sgl::Renderer->createGeometryBuffer(
            (curves.size()+1) * sizeof(uint32_t), &lineOffsets.front(),
            sgl::SHADER_STORAGE_BUFFER, sgl::BUFFER_STATIC);
    sgl::GeometryBufferPtr numSegmentsBuffer = sgl::Renderer->createGeometryBuffer(
            gridSize1D * sizeof(uint32_t),
            sgl::SHADER_STORAGE_BUFFER, sgl::BUFFER_STATIC);
    GLuint bufferID = ((sgl::GeometryBufferGL*)numSegmentsBuffer.get())->getBuffer();
    glClearNamedBufferData(bufferID, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, (const void*)&zeroData);
    sgl::GeometryBufferPtr lineSegmentsBuffer = sgl::Renderer->createGeometryBuffer(
            maxNumLinesPerVoxel * gridSize1D * sizeof(LineSegmentCompressed),
            sgl::SHADER_STORAGE_BUFFER, sgl::BUFFER_STATIC);

    auto endBuffers = std::chrono::system_clock::now();
    auto elapsedBuffers = std::chrono::duration_cast<std::chrono::milliseconds>(endBuffers - startBuffers);
    sgl::Logfile::get()->writeInfo(std::string() + "Computational time to create buffers: "
                                   + std::to_string(elapsedBuffers.count()));


    // PART 2: Discretize, quantize and voxelize the lines.
    auto startVoxelize = std::chrono::system_clock::now();
    unsigned int numWorkGroupsLines = sgl::iceil(curves.size(), 256);
    sgl::ShaderProgramPtr discretizeLinesShader = sgl::ShaderManager->getShaderProgram({"DiscretizeLines.Compute"});
    sgl::ShaderManager->bindShaderStorageBuffer(2, linePointBuffer);
    sgl::ShaderManager->bindShaderStorageBuffer(3, lineOffsetBuffer);
    sgl::ShaderManager->bindShaderStorageBuffer(4, numSegmentsBuffer);
    sgl::ShaderManager->bindShaderStorageBuffer(5, lineSegmentsBuffer);
    discretizeLinesShader->setUniform("numLines", (unsigned int)curves.size());
    discretizeLinesShader->dispatchCompute(numWorkGroupsLines);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // End of PART 2: Read the line segment buffer & number of line segments per voxel buffer back from the GPU.
    std::vector<LineSegmentCompressed> compressedLineSegments;
    bufferMemory = linePointBuffer->mapBuffer(sgl::BUFFER_MAP_READ_ONLY);
    compressedLineSegments.resize(maxNumLinesPerVoxel * gridSize1D * sizeof(LineSegmentCompressed));
    memcpy(&compressedLineSegments.front(), bufferMemory, compressedLineSegments.size() * sizeof(LineSegmentCompressed));
    linePointBuffer->unmapBuffer();

    std::vector<uint32_t> numSegmentsPerVoxel;
    bufferMemory = numSegmentsBuffer->mapBuffer(sgl::BUFFER_MAP_READ_ONLY);
    numSegmentsPerVoxel.resize(gridSize1D * sizeof(uint32_t));
    memcpy(&numSegmentsPerVoxel.front(), bufferMemory, numSegmentsPerVoxel.size() * sizeof(uint32_t));
    numSegmentsBuffer->unmapBuffer();

    auto endVoxelize = std::chrono::system_clock::now();
    auto elapsedVoxelize = std::chrono::duration_cast<std::chrono::milliseconds>(endVoxelize - startVoxelize);
    sgl::Logfile::get()->writeInfo(std::string() + "Computational time to voxelize the lines: "
                                   + std::to_string(elapsedVoxelize.count()));


    // PART 3: Compute the densities
    auto startDensity = std::chrono::system_clock::now();

    sgl::TextureSettings densityTextureSettings = sgl::TextureSettings();
    densityTextureSettings.type = sgl::TEXTURE_3D;
    densityTextureSettings.pixelType = GL_FLOAT;
    densityTextureSettings.pixelFormat = GL_RED;
    densityTextureSettings.internalFormat = GL_R32F;
    sgl::TexturePtr densityTexture = sgl::TextureManager->createEmptyTexture(
            gridResolution.x, gridResolution.y, gridResolution.z, densityTextureSettings);
    sgl::ShaderProgramPtr computeDensityShader = sgl::ShaderManager->getShaderProgram({"ComputeDensity.Compute"});
    computeDensityShader->setUniformImageTexture(0, densityTexture, GL_R32F, GL_READ_WRITE, 0, true, 0);
    computeDensityShader->dispatchCompute(numWorkGroupsVoxel.x, numWorkGroupsVoxel.y, numWorkGroupsVoxel.z);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // End of PART 3: Read the density values back from the GPU.
    std::vector<float> voxelDensities;
    voxelDensities.resize(gridSize1D);
    sgl::TextureGL *densityTextureGL = (sgl::TextureGL*)densityTexture.get();
    glGetTextureImage(densityTextureGL->getTexture(), 0, GL_RED, GL_FLOAT,
            sizeof(float) * gridSize1D, (void*)&voxelDensities.front());

    auto endDensity = std::chrono::system_clock::now();
    auto elapsedDensity = std::chrono::duration_cast<std::chrono::milliseconds>(endDensity - startDensity);
    sgl::Logfile::get()->writeInfo(std::string() + "Computational time to compute the densities: "
                                   + std::to_string(elapsedVoxelize.count()));


    // PART 4: Reduce the size of the buffer using a prefix sum (on the CPU for now).
    auto startPrefixSum = std::chrono::system_clock::now();

    uint32_t lineSegmentOffset = 0;
    std::vector<uint32_t> lineSegmentOffsets;
    std::vector<LineSegmentCompressed> reducedLineSegmentBuffer;
    for (size_t i = 0; i < numSegmentsPerVoxel.size(); i++) {
        lineSegmentOffsets.push_back(lineSegmentOffset);
        size_t numSegmentsCurrentVoxel = numSegmentsPerVoxel.at(i);
        for (size_t j = 0; j < numSegmentsCurrentVoxel; j++) {
            reducedLineSegmentBuffer.push_back(compressedLineSegments.at(i*maxNumLinesPerVoxel+j));
        }
        lineSegmentOffset += numSegmentsCurrentVoxel;
    }

    auto endPrefixSum = std::chrono::system_clock::now();
    auto elapsedPrefixSum = std::chrono::duration_cast<std::chrono::milliseconds>(endVoxelize - startVoxelize);
    sgl::Logfile::get()->writeInfo(std::string() + "Computational time to reduce the buffers: "
                                   + std::to_string(elapsedPrefixSum.count()));


    // PART 5: Compute the ambient occlusion factors. For now, do this on CPU (legacy).
    /*auto startAO_CPU = std::chrono::system_clock::now();

    std::vector<float> voxelAOFactors;
    voxelAOFactors.resize(gridSize1D);
    generateVoxelAOFactorsFromDensity(voxelDensities, voxelAOFactors, gridResolution, isHairDataset);

    auto endAO_CPU = std::chrono::system_clock::now();
    auto elapsedAO_CPU = std::chrono::duration_cast<std::chrono::milliseconds>(endAO_CPU - startAO_CPU);
    sgl::Logfile::get()->writeInfo(std::string() + "Computational time to compute the ambient occlusion factors (CPU): "
                                   + std::to_string(elapsedAO_CPU.count()));*/


    // PART 5: Compute the ambient occlusion factors on the GPU using the density texture.
    auto startAO_GPU = std::chrono::system_clock::now();
    const int FILTER_SIZE = 7;
    const int FILTER_EXTENT = (FILTER_SIZE - 1) / 2;
    const int FILTER_NUM_FIELDS = FILTER_SIZE*FILTER_SIZE*FILTER_SIZE;
    float blurKernel[FILTER_NUM_FIELDS];
    generateGaussianBlurKernel(blurKernel, FILTER_SIZE, FILTER_EXTENT);
    sgl::GeometryBufferPtr gaussianKernelBuffer = sgl::Renderer->createGeometryBuffer(
            FILTER_NUM_FIELDS * sizeof(float), &blurKernel,
            sgl::UNIFORM_BUFFER, sgl::BUFFER_STATIC);

    sgl::TextureSettings aoTextureSettings = sgl::TextureSettings();
    aoTextureSettings.type = sgl::TEXTURE_3D;
    aoTextureSettings.pixelType = GL_FLOAT;
    aoTextureSettings.pixelFormat = GL_RED;
    aoTextureSettings.internalFormat = GL_R32F;
    sgl::TexturePtr aoTexture = sgl::TextureManager->createEmptyTexture(
            gridResolution.x, gridResolution.y, gridResolution.z, aoTextureSettings);
    sgl::ShaderProgramPtr computeAOShader = sgl::ShaderManager->getShaderProgram({"ComputeAO.Compute"});
    computeAOShader->setUniformImageTexture(0, aoTexture, GL_R32F, GL_READ_WRITE, 0, true, 0);
    computeAOShader->setUniform("densityTexture", densityTexture, 0);
    computeAOShader->setUniformBuffer(2, "GaussianFilterWeightBuffer", gaussianKernelBuffer);
    computeAOShader->dispatchCompute(numWorkGroupsVoxel.x, numWorkGroupsVoxel.y, numWorkGroupsVoxel.z);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // End of PART 5: Read the AO factors back from the GPU.
    std::vector<float> voxelAOFactors;
    voxelAOFactors.resize(gridSize1D);
    sgl::TextureGL *aoTextureGL = (sgl::TextureGL*)aoTexture.get();
    glGetTextureImage(aoTextureGL->getTexture(), 0, GL_RED, GL_FLOAT,
                      sizeof(float) * gridSize1D, (void*)&voxelAOFactors.front());
    normalizeVoxelAOFactors(voxelAOFactors, gridResolution, isHairDataset);

    auto endAO_GPU = std::chrono::system_clock::now();
    auto elapsedAO_GPU = std::chrono::duration_cast<std::chrono::milliseconds>(endAO_GPU - startAO_GPU);
    sgl::Logfile::get()->writeInfo(std::string() + "Computational time to compute the ambient occlusion factors (GPU): "
                                   + std::to_string(elapsedAO_GPU.count()));


    // FINAL STEP: Now, write the data to the struct.
    VoxelGridDataCompressed dataCompressed;
    dataCompressed.gridResolution = gridResolution;
    dataCompressed.quantizationResolution = quantizationResolution;
    dataCompressed.worldToVoxelGridMatrix = this->getWorldToVoxelGridMatrix();
    dataCompressed.dataType = isHairDataset ? 1u : 0u;

    if (isHairDataset) {
        dataCompressed.hairStrandColor = hairStrandColor;
        dataCompressed.hairThickness = hairThickness;
    } else {
        dataCompressed.attributes = attributes;
        dataCompressed.maxVorticity = maxVorticity;
    }

    dataCompressed.voxelLineListOffsets = lineSegmentOffsets;
    dataCompressed.numLinesInVoxel = numSegmentsPerVoxel;
    dataCompressed.lineSegments = reducedLineSegmentBuffer;

    dataCompressed.voxelDensityLODs = generateMipmapsForDensity(&voxelDensities.front(), gridResolution);
    dataCompressed.voxelAOLODs = generateMipmapsForDensity(&voxelAOFactors.front(), gridResolution);
    dataCompressed.octreeLODs = generateMipmapsForOctree(&numSegmentsPerVoxel.front(), gridResolution);
    return dataCompressed;
}