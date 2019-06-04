//
// Created by christoph on 04.06.19.
//

#ifndef PIXELSYNCOIT_TRAJECTORYFILE_HPP
#define PIXELSYNCOIT_TRAJECTORYFILE_HPP

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Utils/ImportanceCriteria.hpp"

struct Trajectory {
    std::vector<glm::vec3> positions;
    std::vector<std::vector<float>> attributes;
};

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

#endif //PIXELSYNCOIT_TRAJECTORYFILE_HPP
