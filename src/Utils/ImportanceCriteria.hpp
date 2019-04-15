//
// Created by christoph on 28.02.19.
//

#ifndef PIXELSYNCOIT_IMPORTANCECRITERIA_HPP
#define PIXELSYNCOIT_IMPORTANCECRITERIA_HPP

#include <vector>
#include <glm/glm.hpp>

enum TrajectoryType {
    TRAJECTORY_TYPE_ANEURISM = 0, TRAJECTORY_TYPE_WCB, TRAJECTORY_TYPE_CONVECTION_ROLLS, TRAJECTORY_TYPE_RINGS, TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW
};
enum ImportanceCriterionTypeAneurism {
    IMPORTANCE_CRITERION_ANEURISM_VORTICITY = 0,
    IMPORTANCE_CRITERION_ANEURISM_LINE_CURVATURE
};
enum ImportanceCriterionTypeWCB {
    IMPORTANCE_CRITERION_WCB_CURVATURE = 0,
    IMPORTANCE_CRITERION_SEGMENT_LENGTH,
    IMPORTANCE_CRITERION_WCB_SEGMENT_PRESSURE_DIFFERENCE,
    IMPORTANCE_CRITERION_WCB_TOTAL_PRESSURE_DIFFERENCE,
    IMPORTANCE_CRITERION_WCB_ANGLE_OF_ASCENT,
    IMPORTANCE_CRITERION_WCB_HEIGHT_DIFFERENCE_PER_SEGMENT,
};
enum ImportanceCriterionTypeConvectionRolls {
    IMPORTANCE_CRITERION_CONVECTION_ROLLS_VORTICITY = 0,
    IMPORTANCE_CRITERION_CONVECTION_ROLLS_LINE_CURVATURE,
    IMPORTANCE_CRITERION_CONVECTION_ROLLS_SEGMENT_LENGTH
};
const char *const IMPORTANCE_CRITERION_ANEURISM_DISPLAYNAMES[] = {
        "Vorticity", "Line Curvature"
};
const char *const IMPORTANCE_CRITERION_WCB_DISPLAYNAMES[] = {
        "Pressure", "Line Curvature"
//        "Line Curvature", "Segment Length", "Segment Pressure Difference", "Total Pressure Difference",
//        "Angle of Ascent", "Height Difference per Segment"
};
const char *const IMPORTANCE_CRITERION_CONVECTION_ROLLS_DISPLAYNAMES[] = {
        "Vorticity", "Line Curvature", "Segment Length"
};


/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/packUnorm.xhtml
void packUnorm16ArrayOfArrays(
        std::vector<std::vector<float>> &floatVector,
        std::vector<std::vector<uint16_t>> &unormVector);

/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/unpackUnorm.xhtml
void unpackUnorm16Array(uint16_t *unormVector, size_t vectorSize, std::vector<float> &floatVector);

void computeTrajectoryAttributes(
        TrajectoryType trajectoryType,
        std::vector<glm::vec3> &vertexPositions,
        std::vector<float> &vertexAttributes,
        std::vector<std::vector<float>> &importanceCriteria);

#endif //PIXELSYNCOIT_IMPORTANCECRITERIA_HPP
