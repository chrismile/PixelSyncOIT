//
// Created by christoph on 08.09.18.
//

#ifndef PIXELSYNCOIT_TRAJECTORYLOADER_HPP
#define PIXELSYNCOIT_TRAJECTORYLOADER_HPP

#include <vector>
#include <map>
#include <string>

#include <glm/glm.hpp>

#include "ImportanceCriteria.hpp"

/**
 * Creates normals for the specified indexed vertex set.
 * NOTE: If a vertex is indexed by more than one triangle, then the average normal is stored per vertex.
 * If you want to have non-smooth normals, then make sure each vertex is only referenced by one face.
 */
void createNormals(const std::vector<glm::vec3> &vertices,
                   const std::vector<uint32_t> &indices,
                   std::vector<glm::vec3> &normals);

/**
 * @param pathLineCenters: The (input) path line points to create a tube from.
 * @param pathLineAttributes: The (input) path line point vertex attributes (belonging to pathLineCenters).
 * @param vertices: The (output) vertex points, which are a set of oriented circles around the centers (see above).
 * @param indices: The (output) indices specifying how tube triangles are built from the circle vertices.
 */
template<typename T>
void createTubeRenderData(const std::vector<glm::vec3> &pathLineCenters,
                          const std::vector<T> &pathLineAttributes,
                          std::vector<glm::vec3> &vertices,
                          std::vector<glm::vec3> &normals,
                          std::vector<T> &vertexAttributes,
                          std::vector<uint32_t> &indices);
/*extern template
void createTubeRenderData<uint32_t>(const std::vector<glm::vec3> &pathLineCenters,
                                    const std::vector<uint32_t> &pathLineAttributes,
                                    std::vector<glm::vec3> &vertices,
                                    std::vector<float> &vertexAttributes,
                                    std::vector<uint32_t> &indices);*/
extern template
void createTubeRenderData<uint32_t>(const std::vector<glm::vec3> &pathLineCenters,
                                    const std::vector<uint32_t> &pathLineAttributes,
                                    std::vector<glm::vec3> &vertices,
                                    std::vector<glm::vec3> &normals,
                                    std::vector<uint32_t> &vertexAttributes,
                                    std::vector<uint32_t> &indices);

void initializeCircleData(int numSegments, float radius);

void convertObjTrajectoryDataToBinaryTriangleMesh(
        TrajectoryType trajectoryType,
        const std::string &objFilename,
        const std::string &binaryFilename);

void convertObjTrajectoryDataToBinaryTriangleMeshGPU(
        TrajectoryType trajectoryType,
        const std::string &objFilename,
        const std::string &binaryFilename);

void convertObjTrajectoryDataToBinaryLineMesh(
        TrajectoryType trajectoryType,
        const std::string &objFilename,
        const std::string &binaryFilename);

#endif //PIXELSYNCOIT_TRAJECTORYLOADER_HPP
