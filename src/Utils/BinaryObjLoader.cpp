/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <Utils/File/Logfile.hpp>
#include "MeshSerializer.hpp"
#include "ComputeNormals.hpp"
#include "ImportanceCriteria.hpp"
#include "BinaryObjLoader.hpp"

void convertBinaryObjMeshToBinmesh(
        const std::string &bobjFilename,
        const std::string &binaryFilename)
{
    std::ifstream fin(bobjFilename.c_str(), std::ios::binary);
    if (!fin.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in convertBinaryObjMeshToBinmesh: File \""
                + bobjFilename + "\" does not exist.");
        return;
    }
    sgl::Logfile::get()->writeInfo(std::string() + "Loading binary OBJ mesh from \"" + bobjFilename + "\"...");

    // Loading code by Will
    uint64_t header[2] = {0};
    fin.read(reinterpret_cast<char*>(header), sizeof(header));
    std::vector<glm::vec3> vertices(header[0], glm::vec3(0.0f));
    fin.read(reinterpret_cast<char*>(vertices.data()), sizeof(float) * 3 * header[0]);
    std::vector<uint64_t> indices(header[1] * 3, 0);
    fin.read(reinterpret_cast<char*>(indices.data()), sizeof(uint64_t) * 3 * header[1]);

    // Normalize vertices and flip axis
    // 1) Compute bounding box
    sgl::AABB3 bbox;
    for (auto& vertex : vertices)
    {
        // flip z and y-axis
        float z = vertex.z;
        vertex.z = vertex.y;
        vertex.y = z;

        bbox.combine(vertex);
    }

    // 2) Normalize values to range (-1, 1)
    glm::vec3 center = bbox.getCenter();
    glm::vec3 extent = bbox.getExtent();
    float maxExtent = std::max(extent.x, std::max(extent.y, extent.z));

    for (auto& vertex : vertices)
    {
        vertex = (vertex - center) / maxExtent;
    }

    // 2.5) Flip y-axis
    for (auto& vertex : vertices)
    {
//        vertex.y = bbox.max.y - vertex.y + bbox.min.y;
        vertex.y = -vertex.y;
    }

    // The indices are 64-bit, however, OpenGL currently only supports 32-bit indices. Check if 32-bit is enough.
    sgl::Logfile::get()->writeInfo(std::string() + "Computing additional mesh data...");
    if (vertices.size() / 3 > UINT32_MAX) {
        sgl::Logfile::get()->writeError(std::string() + "Error in convertBinaryObjMeshToBinmesh: File \""
                + bobjFilename + "\" has more than UINT32_MAX vertices (not supported currently).");
        return;
    }

    // Convert indices to 32-bit values for the mesh.
    std::vector<uint32_t> indices32(indices.size());//header[1] * 3, 0);
    #pragma omp parallel for
    for (size_t i = 0; i < indices.size(); i++) {
        indices32[i] = static_cast<uint32_t>(indices[i]);
    }
    // Free the memory.
    indices.clear();
    indices.shrink_to_fit();

    // Compute the normals for our mesh.
    std::vector<glm::vec3> normals;
    std::vector<float> attributes;
    computeNormals(vertices, indices32, normals, attributes);

    // Create a binary mesh from the data.
    BinaryMesh binaryMesh;
    binaryMesh.submeshes.resize(1);

    BinarySubMesh &binarySubmesh = binaryMesh.submeshes.at(0);
    binarySubmesh.vertexMode = sgl::VERTEX_MODE_TRIANGLES;
    binarySubmesh.indices = indices32;

    BinaryMeshAttribute positionAttribute;
    positionAttribute.name = "vertexPosition";
    positionAttribute.attributeFormat = sgl::ATTRIB_FLOAT;
    positionAttribute.numComponents = 3;
    positionAttribute.data.resize(vertices.size() * sizeof(glm::vec3));
    std::memcpy(&positionAttribute.data.front(), vertices.data(), vertices.size() * sizeof(glm::vec3));
    binarySubmesh.attributes.push_back(positionAttribute);

    vertices.clear(); vertices.shrink_to_fit();
    indices32.clear(); indices32.shrink_to_fit();

    BinaryMeshAttribute normalAttribute;
    normalAttribute.name = "vertexNormal";
    normalAttribute.attributeFormat = sgl::ATTRIB_FLOAT;
    normalAttribute.numComponents = 3;
    normalAttribute.data.resize(normals.size() * sizeof(glm::vec3));
    std::memcpy(&normalAttribute.data.front(), &normals.front(), normals.size() * sizeof(glm::vec3));
    binarySubmesh.attributes.push_back(normalAttribute);

    normals.clear(); normals.shrink_to_fit();

    BinaryMeshAttribute vertexAttribute;
    vertexAttribute.name = "vertexAttribute0";
    vertexAttribute.attributeFormat = sgl::ATTRIB_UNSIGNED_SHORT;
    vertexAttribute.numComponents = 1;
    std::vector<uint16_t> vertexAttributeData(attributes.size(), 0u); // Just zero for now
//    vertexAttribute.data.resize(vertices.size() * sizeof(uint16_t), 0);
    packUnorm16Array(attributes, vertexAttributeData);
    vertexAttribute.data.resize(vertexAttributeData.size() * sizeof(uint16_t));
    std::memcpy(&vertexAttribute.data.front(), &vertexAttributeData.front(), vertexAttributeData.size() * sizeof(uint16_t));
    binarySubmesh.attributes.push_back(vertexAttribute);

    attributes.clear(); attributes.shrink_to_fit();
    vertexAttributeData.clear(); vertexAttributeData.shrink_to_fit();

    sgl::Logfile::get()->writeInfo(std::string() + "Writing binary mesh...");
    writeMesh3D(binaryFilename, binaryMesh);
    sgl::Logfile::get()->writeInfo(std::string() + "Finished writing binary mesh.");
}