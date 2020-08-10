//
// Created by christoph on 28.02.19.
//

#ifndef PIXELSYNCOIT_IMPORTANCECRITERIA_HPP
#define PIXELSYNCOIT_IMPORTANCECRITERIA_HPP

#include <vector>
#include <glm/glm.hpp>

enum TrajectoryType {
    TRAJECTORY_TYPE_ANEURYSM = 0, TRAJECTORY_TYPE_WCB, TRAJECTORY_TYPE_CONVECTION_ROLLS, TRAJECTORY_TYPE_RINGS,
    TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW, TRAJECTORY_TYPE_CFD, TRAJECTORY_TYPE_UCLA, TRAJECTORY_TYPE_MULTIVAR
};
enum ImportanceCriterionTypeAneurysm {
    IMPORTANCE_CRITERION_ANEURYSM_VORTICITY = 0,
    IMPORTANCE_CRITERION_ANEURYSM_LINE_CURVATURE
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

enum ImportanceCriterionTypeUCLA {
    IMPORTANCE_CRITERION_UCLA_MAGNITUDE = 0,
    IMPORTANCE_CRITERION_UCLA_LINE_CURVATURE,
    IMPORTANCE_CRITERION_UCLA_SEGMENT_LENGTH
};

enum ImportanceCriterionTypeCFD {
    IMPORTANCE_CRITERION_CFD_CURL = 0,
    IMPORTANCE_CRITERION_CFD_VELOCITY,
};

enum MultiVarRenderModeType {
//    MULTIVAR_RENDERMODE_RIBBONS = 0,
//    MULTIVAR_RENDERMODE_RIBBONS_FIBERS,
//    MULTIVAR_RENDERMODE_STAR_GLYPHS,
//    MULTIVAR_RENDERMODE_TUBE_ROLLS,
//    MULTIVAR_RENDERMODE_LINE,
//    MULTIVAR_RENDERMODE_LINE_INSTANCED,
    MULTIVAR_RENDERMODE_ROLLS = 0,
    MULTIVAR_RENDERMODE_TWISTED_ROLLS,
    MULTIVAR_RENDERMODE_COLOR_BANDS,
    MULTIVAR_RENDERMODE_ORIENTED_COLOR_BANDS,
    MULTIVAR_RENDERMODE_CHECKERBOARD,
    MULTIVAR_RENDERMODE_FIBERS
};

const char *const IMPORTANCE_CRITERION_ANEURYSM_DISPLAYNAMES[] = {
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

const char *const IMPORTANCE_CRITERION_CFD_DISPLAYNAMES[] = {
        "Vorticity", "Velocity Magnitude"
};

const char *const MULTIVAR_RENDERTYPE_DISPLAYNAMES[] = {
//        "Ribbons", "Fibers", "Star Glyphs", "Tube Rolls", "Line", "Line Instanced"
    "Rolls", "Twisted Rolls", "Color Bands", "Oriented Color Bands", "Checkerboard", "Fibers"
};


/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/packUnorm.xhtml
void packUnorm16Array(const std::vector<float> &floatVector, std::vector<uint16_t> &unormVector);

/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/packUnorm.xhtml
void packUnorm16ArrayOfArrays(
        const std::vector<std::vector<float>> &floatVector,
        std::vector<std::vector<uint16_t>> &unormVector);

/// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/unpackUnorm.xhtml
void unpackUnorm16Array(uint16_t *unormVector, size_t vectorSize, std::vector<float> &floatVector);

void computeTrajectoryAttributes(
        TrajectoryType trajectoryType,
        std::vector<glm::vec3> &vertexPositions,
        std::vector<float> &vertexAttributes,
        std::vector<std::vector<float>> &importanceCriteria);

#endif //PIXELSYNCOIT_IMPORTANCECRITERIA_HPP
