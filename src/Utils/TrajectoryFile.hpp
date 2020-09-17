//
// Created by christoph on 04.06.19.
//

#ifndef PIXELSYNCOIT_TRAJECTORYFILE_HPP
#define PIXELSYNCOIT_TRAJECTORYFILE_HPP

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Utils/ImportanceCriteria.hpp"

// If nothing works --> pad data to vec4
struct MultiVarArray
{
    float value;
};

// Describes the position of variables in the buffer and the total number of variable values for the entire line
struct LineDesc
{
    float startIndex; // pointer to index in array
    float numValues;     // number of variables along line
};

// Describes the range of values for each variable and the offset within each line
struct VarDesc
{
    float startIndex;
    glm::vec2 minMax;
    float dummy;
};

class Trajectory {
public:
    explicit Trajectory() {}

    std::vector<glm::vec3> positions;
    std::vector<std::vector<float>> attributes;
    std::vector<std::vector<float>> histograms;
    std::vector<glm::vec3> tangents;
    std::vector<uint32_t> segmentID;

    std::vector<float> multiVarData;
    std::vector<float> multiVarHistograms;
    uint8_t numBins;
    LineDesc lineDesc;
    std::vector<VarDesc> multiVarDescs;
    std::vector<std::string> multiVarNames;
};

//class BezierTrajectory : public Trajectory {
//public:
//    explicit BezierTrajectory() : Trajectory() {}
//
//    std::vector<glm::vec3> tangents;
//    std::vector<uint32_t> segmentID;
//};

typedef std::vector<Trajectory> Trajectories;

/**
 * Selects loadTrajectoriesFromObj, loadTrajectoriesFromNetCdf or loadTrajectoriesFromBinLines depending on the file
 * endings and performs some normalization for special datasets (e.g. the rings dataset).
 * @param filename The name of the trajectory file to open.
 * @return The trajectories loaded from the file (empty if the file could not be opened).
 */
Trajectories loadTrajectoriesFromFile(const std::string &filename, TrajectoryType trajectoryType);

Trajectories loadTrajectoriesFromObj(const std::string &filename, TrajectoryType trajectoryType);

Trajectories loadTrajectoriesFromNetCdf(const std::string &filename, TrajectoryType trajectoryType);

Trajectories loadTrajectoriesFromBinLines(const std::string &filename, TrajectoryType trajectoryType);

Trajectories convertTrajectoriesToBezierCurves(const Trajectories& inTrajectories);

#endif //PIXELSYNCOIT_TRAJECTORYFILE_HPP
