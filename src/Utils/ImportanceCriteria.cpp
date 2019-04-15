//
// Created by christoph on 28.02.19.
//

#include <Math/Math.hpp>
#include "ImportanceCriteria.hpp"

/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/packUnorm.xhtml
void packUnorm16Array(std::vector<float> &floatVector, std::vector<uint16_t> &unormVector)
{
    float minValue = FLT_MAX;
    float maxValue = -FLT_MAX;
    #pragma omp parallel for reduction(min:minValue) reduction(max:maxValue)
    for (size_t i = 0; i < floatVector.size(); i++) {
        minValue = std::min(minValue, floatVector.at(i));
        maxValue = std::max(maxValue, floatVector.at(i));
    }

    unormVector.resize(floatVector.size());
    #pragma omp parallel for
    for (size_t i = 0; i < unormVector.size(); i++) {
        unormVector.at(i) = glm::round(glm::clamp((floatVector.at(i) - minValue) / (maxValue - minValue),
                                                  0.0f, 1.0f) * 65535.0f);
    }
}

/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/packUnorm.xhtml
void packUnorm16ArrayOfArrays(
        std::vector<std::vector<float>> &floatVector,
        std::vector<std::vector<uint16_t>> &unormVector)
{
    unormVector.resize(floatVector.size());
    for (size_t i = 0; i < unormVector.size(); i++) {
        packUnorm16Array(floatVector.at(i), unormVector.at(i));
    }
}


/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/unpackUnorm.xhtml
void unpackUnorm16Array(uint16_t *unormVector, size_t vectorSize, std::vector<float> &floatVector)
{
    floatVector.resize(vectorSize);
    #pragma omp parallel for
    for (size_t i = 0; i < vectorSize; i++) {
        floatVector.at(i) = unormVector[i]/65535.0;
    }
}


std::vector<float> computeSegmentLengths(std::vector<glm::vec3> &vertexPositions)
{
    int n = (int)vertexPositions.size();
    std::vector<float> segmentLengths;
    segmentLengths.reserve(n);

    for (int i = 0; i < n; i++) {
        glm::vec3 tangent;
        if (i == 0) {
            // First node
            tangent = vertexPositions.at(i+1) - vertexPositions.at(i);
        } else if (i == n-1) {
            // Last node
            tangent = vertexPositions.at(i) - vertexPositions.at(i-1);
        } else {
            // Node with two neighbors - use both normals
            tangent = vertexPositions.at(i+1) - vertexPositions.at(i);
            //normal += pathLineCenters.at(i) - pathLineCenters.at(i-1);
        }

        float lineSegmentLength = glm::length(tangent);
        segmentLengths.push_back(lineSegmentLength);
    }

    return segmentLengths;
}

std::vector<float> computeCurvature(std::vector<glm::vec3> &vertexPositions)
{
    int n = (int)vertexPositions.size();
    std::vector<float> curvatures;
    curvatures.reserve(n);

    glm::vec3 tangent, lastTangent = glm::vec3(1.0f, 0.0f, 0.0f);

    for (int i = 0; i < n; i++) {
        float curvatureAngle = 0.0f;

        if (i == 0) {
            // First node
            tangent = vertexPositions.at(i+1) - vertexPositions.at(i);
        } else if (i == n-1) {
            // Last node
            tangent = vertexPositions.at(i) - vertexPositions.at(i-1);
        } else {
            // Node with two neighbors
            tangent = vertexPositions.at(i+1) - vertexPositions.at(i);
        }
        if (glm::length(tangent) < 0.0001f) {
            // In case the two vertices are almost identical, just skip this path line segment
            curvatures.push_back(0.0f);
            continue;
        }

        tangent = glm::normalize(tangent);

        // Compute curvature, i.e. angle between neighboring line segment tangents.
        // Fallback for first and last line point: Assume zero curvature.
        if (i != 0 && i != n-1) {
            float cosAngle = glm::clamp(glm::dot(tangent, lastTangent), 0.0f, 1.0f);
            curvatureAngle = glm::acos(cosAngle) / sgl::PI;
        }

        lastTangent = tangent;
        curvatures.push_back(curvatureAngle);
    }

    return curvatures;
}

std::vector<float> computeSegmentAttributeDifference(
        std::vector<glm::vec3> &vertexPositions,
        std::vector<float> &vertexAttributes)
{
    int n = (int)vertexPositions.size();
    std::vector<float> segmentAttributeDifferences;
    segmentAttributeDifferences.reserve(n);

    for (int i = 0; i < n; i++) {
        float segmentAttributeDifference = 0.0f;

        if (i == 0) {
            // First node
            segmentAttributeDifference = vertexAttributes.at(i+1) - vertexAttributes.at(i);
        } else if (i == n-1) {
            // Last node
            segmentAttributeDifference = vertexAttributes.at(i) - vertexAttributes.at(i-1);
        } else {
            // Node with two neighbors
            segmentAttributeDifference = vertexAttributes.at(i+1) - vertexAttributes.at(i);
        }

        segmentAttributeDifferences.push_back(std::abs(segmentAttributeDifference));
    }

    return segmentAttributeDifferences;
}

std::vector<float> computeTotalAttributeDifference(
        std::vector<glm::vec3> &vertexPositions,
        std::vector<float> &vertexAttributes)
{
    int n = (int)vertexPositions.size();
    std::vector<float> segmentAttributeDifferences;
    segmentAttributeDifferences.reserve(n);

    float minAttribute = FLT_MAX;
    float maxAttribute = -FLT_MAX;
    #pragma omp parallel for reduction(min:minAttribute) reduction(max:maxAttribute)
    for (int i = 0; i < n; i++) {
        minAttribute = std::min(minAttribute, vertexAttributes.at(i));
        maxAttribute = std::max(maxAttribute, vertexAttributes.at(i));
    }

    float totalPressureDifference = maxAttribute - minAttribute;
    for (int i = 0; i < n; i++) {
        segmentAttributeDifferences.push_back(totalPressureDifference);
    }

    return segmentAttributeDifferences;
}

std::vector<float> computeAngleOfAscent(std::vector<glm::vec3> &vertexPositions)
{
    int n = (int)vertexPositions.size();
    std::vector<float> angleOfAscentArray;
    angleOfAscentArray.reserve(n);
    glm::vec3 tangent;
    const glm::vec3 angleUp(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < n; i++) {
        // Angle between line segment and xz-plane
        float angleOfAscent = 0.0f;

        if (i == 0) {
            // First node
            tangent = vertexPositions.at(i+1) - vertexPositions.at(i);
        } else if (i == n-1) {
            // Last node
            tangent = vertexPositions.at(i) - vertexPositions.at(i-1);
        } else {
            // Node with two neighbors
            tangent = vertexPositions.at(i+1) - vertexPositions.at(i);
        }
        if (glm::length(tangent) < 0.0001f) {
            // In case the two vertices are almost identical, just skip this path line segment
            angleOfAscentArray.push_back(0.0f);
            continue;
        }

        tangent = glm::normalize(tangent);
        float cosAngle = glm::clamp(glm::dot(tangent, angleUp), 0.0f, 1.0f);
        angleOfAscent = 1.0f - glm::acos(cosAngle) / sgl::PI;

        angleOfAscentArray.push_back(angleOfAscent);
    }

    return angleOfAscentArray;
}

std::vector<float> computeSegmentHeightDifference(std::vector<glm::vec3> &vertexPositions)
{
    int n = (int)vertexPositions.size();
    std::vector<float> computeSegmentHeightDifferences;
    computeSegmentHeightDifferences.reserve(n);

    for (int i = 0; i < n; i++) {
        float segmentHeightDifference = 0.0f;

        if (i == 0) {
            // First node
            segmentHeightDifference = vertexPositions.at(i+1).y - vertexPositions.at(i).y;
        } else if (i == n-1) {
            // Last node
            segmentHeightDifference = vertexPositions.at(i).y - vertexPositions.at(i-1).y;
        } else {
            // Node with two neighbors
            segmentHeightDifference = vertexPositions.at(i+1).y - vertexPositions.at(i).y;
        }

        computeSegmentHeightDifferences.push_back(segmentHeightDifference);
    }

    return computeSegmentHeightDifferences;
}


void computeTrajectoryAttributes(
        TrajectoryType trajectoryType,
        std::vector<glm::vec3> &vertexPositions,
        std::vector<float> &vertexAttributes,
        std::vector<std::vector<float>> &importanceCriteria)
{
    if (trajectoryType == TRAJECTORY_TYPE_ANEURISM) {
        // 0. Vorticity/Attribute
        importanceCriteria.push_back(vertexAttributes);
//        // 1. Curvature
//        importanceCriteria.push_back(computeCurvature(vertexPositions));
    } else if (trajectoryType == TRAJECTORY_TYPE_WCB) {
        // 0. Pressure mapped to [0, 1]
        importanceCriteria.push_back(vertexAttributes);
        // 1. Curvature
        importanceCriteria.push_back(computeCurvature(vertexPositions));
//        importanceCriteria.push_back(computeSegmentLengths(vertexPositions));
//        // 2. Trajectory pressure/attribute difference per segment
//        importanceCriteria.push_back(computeSegmentAttributeDifference(vertexPositions, vertexAttributes));
//        // 3. Total trajectory pressure/attribute difference
//        importanceCriteria.push_back(computeTotalAttributeDifference(vertexPositions, vertexAttributes));
//        // 4. Angle of ascent
//        importanceCriteria.push_back(computeAngleOfAscent(vertexPositions));
//        // 5. Height difference per segment
//        importanceCriteria.push_back(computeSegmentHeightDifference(vertexPositions));
    } else if (trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS) {
        // 0. Vorticity/Attribute
        importanceCriteria.push_back(vertexAttributes);
        // 1. Curvature
//        importanceCriteria.push_back(computeCurvature(vertexPositions));
//        // 2. Segment length
//        importanceCriteria.push_back(computeSegmentLengths(vertexPositions));
    } else if (trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW) {
        // 0. Vorticity/Attribute
        importanceCriteria.push_back(vertexAttributes);
        // 1. Curvature
//        importanceCriteria.push_back(computeCurvature(vertexPositions));
//        // 2. Segment length
//        importanceCriteria.push_back(computeSegmentLengths(vertexPositions));
    } else if (trajectoryType == TRAJECTORY_TYPE_RINGS) {
        // 0. Vorticity/Attribute
        importanceCriteria.push_back(vertexAttributes);
//
//        importanceCriteria.push_back(computeCurvature(vertexPositions));
//
//        importanceCriteria.push_back(computeSegmentLengths(vertexPositions));
    }
}
