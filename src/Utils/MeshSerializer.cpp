/*
 * MeshSerializer.cpp
 *
 *  Created on: 18.05.2018
 *      Author: christoph
 */

#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <cmath>

#include <boost/algorithm/string/predicate.hpp>
#include <glm/glm.hpp>

#include <Utils/Events/Stream/Stream.hpp>
#include <Utils/File/Logfile.hpp>
#include <Utils/Convert.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>
#include <Graphics/Renderer.hpp>

#include "ImportanceCriteria.hpp"
#include "MeshSerializer.hpp"

using namespace std;
using namespace sgl;

const uint32_t MESH_FORMAT_VERSION = 4u;

void writeMesh3D(const std::string &filename, const BinaryMesh &mesh) {
#ifndef __MINGW32__
    std::ofstream file(filename.c_str(), std::ofstream::binary);
    if (!file.is_open()) {
        Logfile::get()->writeError(std::string() + "Error in writeMesh3D: File \"" + filename + "\" not found.");
        return;
    }
 #else
    FILE *fileptr = fopen(filename.c_str(), "wb");
    if (fileptr == NULL) {
        Logfile::get()->writeError(std::string() + "Error in writeMesh3D: File \"" + filename + "\" not found.");
        return;
    }
 #endif

    sgl::BinaryWriteStream stream;
    stream.write((uint32_t)MESH_FORMAT_VERSION);
    stream.write((uint32_t)mesh.submeshes.size());

    for (const BinarySubMesh &submesh : mesh.submeshes) {
        stream.write(submesh.material);
        stream.write((uint32_t)submesh.vertexMode);
        stream.writeArray(submesh.indices);

        // Write attributes
        stream.write((uint32_t)submesh.attributes.size());
        for (const BinaryMeshAttribute &attribute : submesh.attributes) {
            stream.write(attribute.name);
            stream.write((uint32_t)attribute.attributeFormat);
            stream.write((uint32_t)attribute.numComponents);
            stream.writeArray(attribute.data);
        }

        // Write uniforms
        stream.write((uint32_t)submesh.uniforms.size());
        for (const BinaryMeshUniform &uniform : submesh.uniforms) {
            stream.write(uniform.name);
            stream.write((uint32_t)uniform.attributeFormat);
            stream.write((uint32_t)uniform.numComponents);
            stream.writeArray(uniform.data);
        }
    }

#ifndef __MINGW32__
    file.write((const char*)stream.getBuffer(), stream.getSize());
    file.close();
#else
    fwrite((const void*)stream.getBuffer(), stream.getSize(), 1, fileptr);
    fclose(fileptr);
#endif
}

void readMesh3D(const std::string &filename, BinaryMesh &mesh) {
#ifndef __MINGW32__
    std::ifstream file(filename.c_str(), std::ifstream::binary);
    if (!file.is_open()) {
        Logfile::get()->writeError(std::string() + "Error in readMesh3D: File \"" + filename + "\" not found.");
        return;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0);
    char *buffer = new char[size];
    file.read(buffer, size);
    file.close();
#else
    /*
     * Using MinGW-w64, the code above doesn't work, as streamsize == ptrdiff_t == __PTRDIFF_TYPE__ == long int
     * is only 32-bits long, and thus is is faulty for large files > 2GiB.
     */
    FILE *fileptr = fopen(filename.c_str(), "rb");
    if (fileptr == NULL) {
        Logfile::get()->writeError(std::string() + "Error in readMesh3D: File \"" + filename + "\" not found.");
        return;
    }
    fseeko64(fileptr, 0L, SEEK_END);
    size_t size = ftello64 (fileptr);
    fseeko64(fileptr, 0L, SEEK_SET);
    char *buffer = new char[size];
    fseek(fileptr, 0L, SEEK_SET);
    uint64_t readSize = fread(buffer, 1, size, fileptr);
    assert(size == readSize);
    fclose(fileptr);
#endif

    sgl::BinaryReadStream stream(buffer, size);
    uint32_t version;
    stream.read(version);
    if (version != MESH_FORMAT_VERSION) {
        Logfile::get()->writeError(std::string() + "Error in readMesh3D: Invalid version in file \""
                + filename + "\".");
        return;
    }

    uint32_t numSubmeshes;
    stream.read(numSubmeshes);
    mesh.submeshes.resize(numSubmeshes);

    for (uint32_t i = 0; i < numSubmeshes; i++) {
        BinarySubMesh &submesh = mesh.submeshes.at(i);
        stream.read(submesh.material);
        uint32_t vertexMode;
        stream.read(vertexMode);
        submesh.vertexMode = (sgl::VertexMode)vertexMode;
        stream.readArray(mesh.submeshes.at(i).indices);

        // Read attributes
        uint32_t numAttributes;
        stream.read(numAttributes);
        submesh.attributes.resize(numAttributes);

        for (uint32_t j = 0; j < numAttributes; j++) {
            BinaryMeshAttribute &attribute = submesh.attributes.at(j);
            stream.read(attribute.name);
            uint32_t format;
            stream.read(format);
            attribute.attributeFormat = (sgl::VertexAttributeFormat)format;
            stream.read(attribute.numComponents);
            stream.readArray(attribute.data);
        }

        // Read uniforms
        uint32_t numUniforms;
        stream.read(numUniforms);
        submesh.uniforms.resize(numUniforms);

        for (uint32_t j = 0; j < numUniforms; j++) {
            BinaryMeshUniform &uniform = submesh.uniforms.at(j);
            stream.read(uniform.name);
            uint32_t format;
            stream.read(format);
            uniform.attributeFormat = (sgl::VertexAttributeFormat)format;
            stream.read(uniform.numComponents);
            stream.readArray(uniform.data);
        }
    }

    //delete[] buffer; // BinaryReadStream does deallocation
}





void MeshRenderer::render(sgl::ShaderProgramPtr passShader, bool isGBufferPass)
{
    for (size_t i = 0; i < shaderAttributes.size(); i++) {
        //ShaderProgram *shader = shaderAttributes.at(i)->getShaderProgram();
        if (!boost::starts_with(passShader->getShaderList().front()->getFileID(), "PseudoPhongVorticity")
                && !boost::starts_with(passShader->getShaderList().front()->getFileID(), "DepthPeelingGatherDepthComplexity")
                && !isGBufferPass) {
            if (passShader->hasUniform("ambientColor")) {
                passShader->setUniform("ambientColor", materials.at(i).ambientColor);
            }
            if (passShader->hasUniform("diffuseColor")) {
                passShader->setUniform("diffuseColor", materials.at(i).diffuseColor);
            }
            if (passShader->hasUniform("specularColor")) {
                passShader->setUniform("specularColor", materials.at(i).specularColor);
            }
            if (passShader->hasUniform("specularExponent")) {
                passShader->setUniform("specularExponent", materials.at(i).specularExponent);
            }
            if (passShader->hasUniform("opacity")) {
                passShader->setUniform("opacity", materials.at(i).opacity);
            }
        }
        Renderer->render(shaderAttributes.at(i), passShader);
    }
}

void MeshRenderer::setNewShader(sgl::ShaderProgramPtr newShader)
{
    for (size_t i = 0; i < shaderAttributes.size(); i++) {
        shaderAttributes.at(i) = shaderAttributes.at(i)->copy(newShader, false);
    }
}


sgl::AABB3 computeAABB(const std::vector<glm::vec3> &vertices)
{
    if (vertices.size() < 1) {
        Logfile::get()->writeError("computeAABB: vertices.size() < 1");
        return sgl::AABB3();
    }

    glm::vec3 minV = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    glm::vec3 maxV = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const glm::vec3 &pt : vertices) {
        minV.x = std::min(minV.x, pt.x);
        minV.y = std::min(minV.y, pt.y);
        minV.z = std::min(minV.z, pt.z);
        maxV.x = std::max(maxV.x, pt.x);
        maxV.y = std::max(maxV.y, pt.y);
        maxV.z = std::max(maxV.z, pt.z);
    }

    return sgl::AABB3(minV, maxV);
}

std::vector<uint32_t> shuffleIndicesLines(const std::vector<uint32_t> &indices) {
    size_t numSegments = indices.size() / 2;
    std::vector<size_t> shuffleOffsets;
    for (size_t i = 0; i < numSegments; i++) {
        shuffleOffsets.push_back(i);
    }
    auto rng = std::default_random_engine{};
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(std::begin(shuffleOffsets), std::end(shuffleOffsets), rng);

    std::vector<uint32_t> shuffledIndices;
    shuffledIndices.reserve(numSegments*2);
    for (size_t i = 0; i < numSegments; i++) {
        size_t lineIndex = shuffleOffsets.at(i);
        shuffledIndices.push_back(indices.at(lineIndex*2));
        shuffledIndices.push_back(indices.at(lineIndex*2+1));
    }

    return shuffledIndices;
}

std::vector<uint32_t> shuffleLineOrder(const std::vector<uint32_t> &indices) {
    size_t numSegments = indices.size() / 2;

    // 1. Compute list of all lines
    std::vector<std::vector<uint32_t>> lines;
    std::vector<uint32_t> currentLine;
    for (size_t i = 0; i < numSegments; i++) {
        uint32_t idx0 = indices.at(i*2);
        uint32_t idx1 = indices.at(i*2+1);

        // Start new line?
        if (i > 0 && idx0 != indices.at((i-1)*2+1)) {
            lines.push_back(currentLine);
            currentLine.clear();
        }

        // Add indices to line
        currentLine.push_back(idx0);
        currentLine.push_back(idx1);
    }
    if (currentLine.size() > 0) {
        lines.push_back(currentLine);
    }

    // 2. Shuffle line list
    auto rng = std::default_random_engine{};
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(std::begin(lines), std::end(lines), rng);

    // 3. Reconstruct line list from shuffled lines
    std::vector<uint32_t> shuffledIndices;
    shuffledIndices.reserve(indices.size());
    for (const std::vector<uint32_t> &line : lines) {
        for (uint32_t idx : line) {
            shuffledIndices.push_back(idx);
        }
    }

    return shuffledIndices;
}

std::vector<uint32_t> shuffleIndicesTriangles(const std::vector<uint32_t> &indices) {
    size_t numSegments = indices.size() / 3;
    std::vector<size_t> shuffleOffsets;
    for (size_t i = 0; i < numSegments; i++) {
        shuffleOffsets.push_back(i);
    }
    auto rng = std::default_random_engine{};
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(std::begin(shuffleOffsets), std::end(shuffleOffsets), rng);

    std::vector<uint32_t> shuffledIndices;
    shuffledIndices.reserve(numSegments*3);
    for (size_t i = 0; i < numSegments; i++) {
        size_t lineIndex = shuffleOffsets.at(i);
        shuffledIndices.push_back(indices.at(lineIndex*3));
        shuffledIndices.push_back(indices.at(lineIndex*3+1));
        shuffledIndices.push_back(indices.at(lineIndex*3+2));
    }

    return shuffledIndices;
}

MeshRenderer parseMesh3D(const std::string &filename, sgl::ShaderProgramPtr shader, bool shuffleData)
{
    MeshRenderer meshRenderer;
    BinaryMesh mesh;
    readMesh3D(filename, mesh);

    if (!shader) {
        shader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    }

    std::vector<sgl::ShaderAttributesPtr> &shaderAttributes = meshRenderer.shaderAttributes;
    std::vector<ObjMaterial> &materials = meshRenderer.materials;
    shaderAttributes.reserve(mesh.submeshes.size());
    materials.reserve(mesh.submeshes.size());

    // Bounding box of all submeshes combined
    AABB3 totalBoundingBox(glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX), glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX));

    // Importance criterion attributes are bound to location 3 and onwards in vertex shader
    //int importanceCriterionLocationCounter = 3;

    // Iterate over all submeshes and create rendering data
    for (size_t i = 0; i < mesh.submeshes.size(); i++) {
        BinarySubMesh &submesh = mesh.submeshes.at(i);
        ShaderAttributesPtr renderData = ShaderManager->createShaderAttributes(shader);
        renderData->setVertexMode(submesh.vertexMode);
        if (submesh.indices.size() > 0) {
            if (shuffleData && (submesh.vertexMode == VERTEX_MODE_LINES || submesh.vertexMode == VERTEX_MODE_TRIANGLES)) {
                std::vector<uint32_t> shuffledIndices;
                if (submesh.vertexMode == VERTEX_MODE_LINES) {
                    //shuffledIndices = shuffleIndicesLines(submesh.indices);
                    shuffledIndices = shuffleLineOrder(submesh.indices);
                } else if (submesh.vertexMode == VERTEX_MODE_TRIANGLES) {
                    shuffledIndices = shuffleIndicesTriangles(submesh.indices);
                } else {
                    Logfile::get()->writeError("ERROR in parseMesh3D: shuffleData and unsupported vertex mode!");
                    shuffledIndices = submesh.indices;
                }
                GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(
                        sizeof(uint32_t)*shuffledIndices.size(), (void*)&shuffledIndices.front(), INDEX_BUFFER);
                renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);
            } else {
                GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(
                        sizeof(uint32_t)*submesh.indices.size(), (void*)&submesh.indices.front(), INDEX_BUFFER);
                renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);
            }
        }

        for (size_t j = 0; j < submesh.attributes.size(); j++) {
            BinaryMeshAttribute &meshAttribute = submesh.attributes.at(j);

            // Assume only one component means importance criterion like vorticity, line width, ...
            if (meshAttribute.numComponents == 1) {
                ImportanceCriterionAttribute importanceCriterionAttribute;
                importanceCriterionAttribute.name = meshAttribute.name;

                // Copy values to mesh renderer data structure
                uint16_t *attributeValuesUnorm = (uint16_t*)&meshAttribute.data.front();
                size_t numAttributeValues = meshAttribute.data.size() / sizeof(uint16_t);
                unpackUnorm16Array(attributeValuesUnorm, numAttributeValues, importanceCriterionAttribute.attributes);

                // Compute minimum and maximum value
                float minValue = FLT_MAX, maxValue = 0.0f;
                #pragma omp parallel for reduction(min:minValue) reduction(max:maxValue)
                for (size_t k = 0; k < numAttributeValues; k++) {
                    minValue = std::min(minValue, importanceCriterionAttribute.attributes[k]);
                    maxValue = std::max(maxValue, importanceCriterionAttribute.attributes[k]);
                }
                importanceCriterionAttribute.minAttribute = minValue;
                importanceCriterionAttribute.maxAttribute = maxValue;

                meshRenderer.importanceCriterionAttributes.push_back(importanceCriterionAttribute);
            }

            GeometryBufferPtr attributeBuffer = Renderer->createGeometryBuffer(
                    meshAttribute.data.size(), (void*)&meshAttribute.data.front(), VERTEX_BUFFER);
            if (meshAttribute.numComponents == 1) {
                // Importance criterion attributes are bound to location 3 and onwards in vertex shader
                /*renderData->addGeometryBuffer(attributeBuffer, importanceCriterionLocationCounter,
                        meshAttribute.attributeFormat, meshAttribute.numComponents, 0, 0, 0, true); */
                renderData->addGeometryBufferOptional(attributeBuffer, meshAttribute.name.c_str(),
                        meshAttribute.attributeFormat, meshAttribute.numComponents, 0, 0, 0, true);
            } else {
                bool isNormalizedColor = (meshAttribute.name == "vertexColor");
                renderData->addGeometryBufferOptional(attributeBuffer, meshAttribute.name.c_str(),
                        meshAttribute.attributeFormat, meshAttribute.numComponents, 0, 0, 0, isNormalizedColor);
            }
            meshRenderer.shaderAttributeNames.insert(meshAttribute.name);

            if (meshAttribute.name == "vertexPosition") {
                std::vector<glm::vec3> vertices;
                vertices.resize(meshAttribute.data.size() / sizeof(glm::vec3));
                memcpy(&vertices.front(), &meshAttribute.data.front(), meshAttribute.data.size());
                totalBoundingBox.combine(computeAABB(vertices));
            }
        }

        shaderAttributes.push_back(renderData);
        materials.push_back(mesh.submeshes.at(i).material);
    }

    meshRenderer.boundingBox = totalBoundingBox;
    meshRenderer.boundingSphere = sgl::Sphere(totalBoundingBox.getCenter(), glm::length(totalBoundingBox.getExtent()));

    return meshRenderer;
}
