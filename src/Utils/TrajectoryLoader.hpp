//
// Created by christoph on 08.09.18.
//

#ifndef PIXELSYNCOIT_TRAJECTORYLOADER_HPP
#define PIXELSYNCOIT_TRAJECTORYLOADER_HPP

#include <vector>
#include <map>
#include <string>

#include <glm/glm.hpp>

void convertObjTrajectoryDataToBinaryTriangleMesh(
        const std::string &objFilename,
        const std::string &binaryFilename);

void convertObjTrajectoryDataToBinaryLineMesh(
        const std::string &objFilename,
        const std::string &binaryFilename);

#endif //PIXELSYNCOIT_TRAJECTORYLOADER_HPP
