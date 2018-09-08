//
// Created by christoph on 08.09.18.
//

#ifndef PIXELSYNCOIT_TRAJECTORYLOADER_HPP
#define PIXELSYNCOIT_TRAJECTORYLOADER_HPP

#include <vector>
#include <map>
#include <string>

#include <glm/glm.hpp>

/**
 * @param pathLineCenters: The (input) path line points to create a tube from.
 * @param vertices: The (output) vertex points, which are a set of oriented circles around the centers (see above).
 * @param indices: The (output) indices specifying how tube triangles are built from the circle vertices.
 */
void createTubeRenderData(const std::vector<glm::vec3> &tubeVertexCenters,
                          std::vector<glm::vec3> &vertices, std::vector<uint32_t> &indices);

void convertObjTrajectoryDataToBinaryMesh(
        const std::string &objFilename,
        const std::string &binaryFilename);

#endif //PIXELSYNCOIT_TRAJECTORYLOADER_HPP
