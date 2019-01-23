//
// Created by christoph on 22.01.19.
//

#ifndef PIXELSYNCOIT_HAIRLOADER_HPP
#define PIXELSYNCOIT_HAIRLOADER_HPP

#include <vector>
#include <map>
#include <string>

#include <glm/glm.hpp>

void convertHairDataToBinaryTriangleMesh(
        const std::string &hairFilename,
        const std::string &binaryFilename);

#endif //PIXELSYNCOIT_HAIRLOADER_HPP
