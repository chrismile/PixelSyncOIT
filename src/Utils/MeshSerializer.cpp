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

        // Write variables
        stream.write((uint32_t)submesh.variables.size());
        for (const BinaryLineVariable &variable : submesh.variables)
        {
            stream.write(variable.name);
            stream.write((uint32_t)variable.attributeFormat);
            stream.write((uint32_t)variable.numComponents);
            stream.writeArray(variable.data);
            stream.writeArray(variable.minValues);
            stream.writeArray(variable.maxValues);
            stream.writeArray(variable.lineOffsets);
            stream.writeArray(variable.varOffsets);
            stream.writeArray(variable.allMinValues);
            stream.writeArray(variable.allMaxValues);
        }

        // Write variables info
        stream.write((uint32_t)submesh.varInfos.size());
        for (const BinaryVariableInfo& info : submesh.varInfos)
        {
            stream.write(info.name);
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

        // Read variables
        uint32_t numVariables;
        stream.read(numVariables);
        submesh.variables.resize(numVariables);

        for (uint32_t j = 0; j < numVariables; j++) {
            BinaryLineVariable &lineVariables = submesh.variables.at(j);
            stream.read(lineVariables.name);
            uint32_t format;
            stream.read(format);
            lineVariables.attributeFormat = (sgl::VertexAttributeFormat)format;
            stream.read(lineVariables.numComponents);
            stream.readArray(lineVariables.data);
            stream.readArray(lineVariables.minValues);
            stream.readArray(lineVariables.maxValues);
            stream.readArray(lineVariables.lineOffsets);
            stream.readArray(lineVariables.varOffsets);
            stream.readArray(lineVariables.allMinValues);
            stream.readArray(lineVariables.allMaxValues);
        }

        uint32_t numVariablesParam = 0;
        stream.read(numVariablesParam);
        submesh.varInfos.resize(numVariablesParam);
        for (uint32_t j = 0; j < numVariablesParam; ++j)
        {
            BinaryVariableInfo& varInfo = submesh.varInfos.at(j);
            stream.read(varInfo.name);
        }

    }

    //delete[] buffer; // BinaryReadStream does deallocation
}





void MeshRenderer::render(sgl::ShaderProgramPtr passShader, bool isGBufferPass, int attributeIndex)
{
    if (useProgrammableFetch) {
        for (SSBOEntry &ssboEntry : ssboEntries) {
            if (ssboEntry.bindingPoint >= 0 && (!boost::starts_with(ssboEntry.attributeName, "vertexAttribute")
                    || attributeIndex == sgl::fromString<int>(ssboEntry.attributeName.substr(15)))) {
                sgl::ShaderManager->bindShaderStorageBuffer(ssboEntry.bindingPoint, ssboEntry.attributeBuffer);
            }
        }
    }

    for (SSBOEntry &ssboEntry : ssboEntries) {
        if (ssboEntry.bindingPoint >= 0) {
            sgl::ShaderManager->bindShaderStorageBuffer(ssboEntry.bindingPoint, ssboEntry.attributeBuffer);
        }
    }

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

void LineMultiVarRenderer::render(sgl::ShaderProgramPtr passShader,
            bool isGBufferPass,
            int attributeIndex)
{
    for (size_t i = 0; i < shaderAttributes.size(); i++) {
        if (!isGBufferPass)
        {
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


struct LinePointData
{
    glm::vec3 vertexPosition;
    float vertexAttribute;
    glm::vec3 vertexTangent;
    float padding;
};

struct LineDescData
{
    float startIndex;
};

struct VarDescData
{
//    float startIndex;
//    glm::vec2 minMax;
//    float padding;
    glm::vec4 info;
};

struct LineVarDescData
{
//    float startIndex;
//    glm::vec2 minMax;
//    float padding;
    glm::vec4 minMax;
};

MeshRenderer parseMesh3D(const std::string &filename, sgl::ShaderProgramPtr shader, bool shuffleData,
        bool useProgrammableFetch, bool programmableFetchUseAoS, float lineRadius, int instancing)
{
    MeshRenderer meshRenderer(useProgrammableFetch);
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
        if (!useProgrammableFetch) {
            renderData->setVertexMode(submesh.vertexMode);
        } else {
            renderData->setVertexMode(VERTEX_MODE_TRIANGLES);
        }

        if (instancing > 0)
        {
            renderData->setInstanceCount(instancing);
        }

        if (submesh.indices.size() > 0 && !useProgrammableFetch) {
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
        if (submesh.indices.size() > 0 && useProgrammableFetch) {
            // Modify indices
            std::vector<uint32_t> fetchIndices;
            fetchIndices.reserve(submesh.indices.size()*3);
            // Iterate over all line segments
            for (size_t i = 0; i < submesh.indices.size(); i += 2) {
                uint32_t base0 = submesh.indices.at(i)*2;
                uint32_t base1 = submesh.indices.at(i+1)*2;
                // 0,2,3,0,3,1
                fetchIndices.push_back(base0);
                fetchIndices.push_back(base1);
                fetchIndices.push_back(base1+1);
                fetchIndices.push_back(base0);
                fetchIndices.push_back(base1+1);
                fetchIndices.push_back(base0+1);
            }
            GeometryBufferPtr indexBuffer = Renderer->createGeometryBuffer(
                    sizeof(uint32_t)*fetchIndices.size(), (void*)&fetchIndices.front(), INDEX_BUFFER);
            renderData->setIndexGeometryBuffer(indexBuffer, ATTRIB_UNSIGNED_INT);
        }

        // For programmableFetchUseAoS
        std::vector<glm::vec3> vertexPositionData;
        std::vector<std::vector<float>> vertexAttributeData;
        std::vector<glm::vec3> vertexTangentData;

        for (size_t j = 0; j < submesh.variables.size(); j++) {
//            continue;
            BinaryLineVariable &lineVariable = submesh.variables.at(j);
//            GeometryBufferPtr attributeBuffer;

            const uint32_t numVariables = lineVariable.numComponents;

            float *allVariablesData = reinterpret_cast<float*>(&lineVariable.data.front());
            uint32_t numVariablesValues = lineVariable.data.size() / sizeof(float);

            // Per variable
            float *minValues = reinterpret_cast<float*>(&lineVariable.minValues.front());
            float *maxValues = reinterpret_cast<float*>(&lineVariable.maxValues.front());
            float *varOffsets = reinterpret_cast<float*>(&lineVariable.varOffsets.front());
            uint32_t numOffsets = lineVariable.varOffsets.size() / sizeof(float);
            // Per line
            float *lineOffsets = reinterpret_cast<float*>(&lineVariable.lineOffsets.front());
            float *allMinValues = reinterpret_cast<float*>(&lineVariable.allMinValues.front());
            float *allMaxValues = reinterpret_cast<float*>(&lineVariable.allMaxValues.front());

            uint32_t minMaxValues = lineVariable.allMinValues.size() / sizeof(float);

            uint32_t numLines = lineVariable.lineOffsets.size() / sizeof(float);

            std::vector<float> varData;
            std::vector<LineDescData> lineDescData(numLines);
            std::vector<VarDescData> varDescData;
            std::vector<LineVarDescData> lineVarDescData;

            for (auto v = 0; v < numVariablesValues; ++v)
            {
                varData.push_back(allVariablesData[v]);
            }

            for (auto l = 0; l < numLines; ++l)
            {
                lineDescData[l].startIndex = lineOffsets[l];
            }

            for (auto var = 0; var < numLines * minMaxValues; ++var)
            {
                uint32_t tempVarID = var % minMaxValues;

                VarDescData descData;
//                descData.startIndex = varOffsets[var];
//                descData.minMax = glm::vec2(allMinValues[tempVarID], allMaxValues[tempVarID]);
//                descData.padding = 0;
                descData.info = glm::vec4(varOffsets[var], allMinValues[tempVarID], allMaxValues[tempVarID], 0.0);
                varDescData.push_back(descData);

                LineVarDescData lineVarDesc;
                lineVarDesc.minMax = glm::vec4(minValues[var], maxValues[var], 0.0, 0.0);
                lineVarDescData.push_back(lineVarDesc);
            }

            GeometryBufferPtr varBuffer = Renderer->createGeometryBuffer(
                    varData.size()*sizeof(float), (void*)&varData.front(),
                    SHADER_STORAGE_BUFFER);
            meshRenderer.ssboEntries.push_back(SSBOEntry(2, "allVariables" ,
                                                         varBuffer));

            GeometryBufferPtr lineDescBuffer = Renderer->createGeometryBuffer(
                    lineDescData.size()*sizeof(LineDescData), (void*)&lineDescData.front(),
                    SHADER_STORAGE_BUFFER);
            meshRenderer.ssboEntries.push_back(SSBOEntry(3, "lineDescs" ,
                                                         lineDescBuffer));

            GeometryBufferPtr varDescBuffer = Renderer->createGeometryBuffer(
                    varDescData.size()*sizeof(VarDescData), (void*)&varDescData.front(),
                    SHADER_STORAGE_BUFFER);
            meshRenderer.ssboEntries.push_back(SSBOEntry(4, "varDescs" ,
                                                         varDescBuffer));

            GeometryBufferPtr lineVarDescBuffer = Renderer->createGeometryBuffer(
                    lineVarDescData.size()*sizeof(LineVarDescData), (void*)&lineVarDescData.front(),
                    SHADER_STORAGE_BUFFER);
            meshRenderer.ssboEntries.push_back(SSBOEntry(5, "lineVarDescs" ,
                                                         lineVarDescBuffer));


//            // Currently all variables are 1-dimensional
//            if (lineVariable.numComponents == 1)
//            {
//                ImportanceCriterionAttribute importanceCriterionAttribute;
//                importanceCriterionAttribute.name = lineVariable.name;
//
//                int64_t numAttributeValues = 0;
//
//                float *attributeValuesUnorm = reinterpret_cast<float*>(&lineVariable.data.front());
//                numAttributeValues = lineVariable.data.size() / sizeof(float);
//
//                importanceCriterionAttribute.attributes.resize(numAttributeValues);
//
//                for (auto a = 0; a < numAttributeValues; ++a)
//                {
//                    importanceCriterionAttribute.attributes[a] = attributeValuesUnorm[a];
//                }
//
//                importanceCriterionAttribute.minAttribute = lineVariable.minValue;
//                importanceCriterionAttribute.maxAttribute = lineVariable.maxValue;
//
//                meshRenderer.importanceCriterionAttributes.push_back(importanceCriterionAttribute);

//                renderData->addGeometryBufferOptional(attributeBuffer, lineVariable.name.c_str(),
//                                                      lineVariable.attributeFormat, lineVariable.numComponents, 0, 0, 0, true);
//            }
        }

        // read variable names
        for (size_t j = 0; j < submesh.varInfos.size(); ++j)
        {
            meshRenderer.varNames.push_back(submesh.varInfos[j].name);
        }

        for (size_t j = 0; j < submesh.attributes.size(); j++) {
            BinaryMeshAttribute &meshAttribute = submesh.attributes.at(j);
            GeometryBufferPtr attributeBuffer;

            // Assume only one component means importance criterion like vorticity, line width, ...
            if (meshAttribute.numComponents == 1) {
                ImportanceCriterionAttribute importanceCriterionAttribute;
                importanceCriterionAttribute.name = meshAttribute.name;

                size_t numAttributeValues = 0;

                // Copy values to mesh renderer data structure
                if (meshAttribute.attributeFormat == ATTRIB_UNSIGNED_SHORT)
                {
                    uint16_t *attributeValuesUnorm = (uint16_t*)&meshAttribute.data.front();
                    numAttributeValues = meshAttribute.data.size() / sizeof(uint16_t);
                    unpackUnorm16Array(attributeValuesUnorm, numAttributeValues, importanceCriterionAttribute.attributes);
                }
                else
                {
                    float *attributeValuesUnorm = reinterpret_cast<float*>(&meshAttribute.data.front());
                    numAttributeValues = meshAttribute.data.size() / sizeof(float);

                    importanceCriterionAttribute.attributes.resize(numAttributeValues);

                    for (auto a = 0; a < numAttributeValues; ++a)
                    {
                        importanceCriterionAttribute.attributes[a] = attributeValuesUnorm[a];
                    }
                }

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

                // SSBOs can't directly perform process uint16_t -> float :(
                if (useProgrammableFetch && !programmableFetchUseAoS) {
                    attributeBuffer = Renderer->createGeometryBuffer(
                            numAttributeValues*sizeof(float), (void*)&importanceCriterionAttribute.attributes.front(),
                            SHADER_STORAGE_BUFFER);
                } else if (useProgrammableFetch) {
                    int attributeIndex = sgl::fromString<int>(meshAttribute.name.substr(15));
                    if (attributeIndex >= vertexAttributeData.size()) {
                        vertexAttributeData.resize(attributeIndex+1);
                    }
                    vertexAttributeData.at(attributeIndex).resize(numAttributeValues);
                    for (size_t k = 0; k < numAttributeValues; k++) {
                        vertexAttributeData.at(attributeIndex).at(k) = importanceCriterionAttribute.attributes[k];
                    }
                }
            }

            BufferType bufferType = useProgrammableFetch ? SHADER_STORAGE_BUFFER : VERTEX_BUFFER;

            if (meshAttribute.name.find("variableDesc") != meshAttribute.name.npos)
            {
                glm::vec4 *attributeValues = (glm::vec4*)&meshAttribute.data.front();
                size_t numAttributeValues = meshAttribute.data.size() / sizeof(glm::vec4);
                std::vector<glm::vec4> vec2AttributeValues;
                vec2AttributeValues.reserve(numAttributeValues);

                for (size_t v = 0; v < numAttributeValues; v++) {
                    vec2AttributeValues.push_back(attributeValues[v]);
                }

//                attributeBuffer = Renderer->createGeometryBuffer(
//                        numAttributeValues * sizeof(glm::vec2),
//                        (void*)&vec2AttributeValues.front(), bufferType);
            }

            if (meshAttribute.name.find("multiVariable") != meshAttribute.name.npos)
            {
                glm::vec4 *attributeValues = (glm::vec4*)&meshAttribute.data.front();
                size_t numAttributeValues = meshAttribute.data.size() / sizeof(glm::vec4);
                std::vector<glm::vec4> vec4AttributeValues;
                vec4AttributeValues.reserve(numAttributeValues);

                // TODO current HACK
                uint32_t maxNumVars = meshRenderer.varNames.size();
                std::vector<std::vector<float>> varValues(maxNumVars);
                std::vector<glm::vec2> minMaxValues(maxNumVars, glm::vec2(0, 0));

                for (size_t i = 0; i < numAttributeValues; i++) {
                    glm::vec4 vec4Value = attributeValues[i];
                    vec4AttributeValues.push_back(vec4Value); // maybe this is unecessary

                    if (vec4Value.w >= 0)
                    {
                        varValues[vec4Value.w].push_back(vec4Value.x);
                        minMaxValues[vec4Value.w] = glm::vec2(vec4Value.y, vec4Value.z);
                    }
                }

                for (auto a = 0; a < maxNumVars; ++a)
                {
                    ImportanceCriterionAttribute importanceCriterionAttribute;
                    importanceCriterionAttribute.name = meshAttribute.name;
                    importanceCriterionAttribute.attributes = varValues[a];
                    importanceCriterionAttribute.minAttribute = minMaxValues[a].x;
                    importanceCriterionAttribute.maxAttribute = minMaxValues[a].y;

                    meshRenderer.importanceCriterionAttributes.push_back(importanceCriterionAttribute);
                }


                for (size_t i = 0; i < numAttributeValues; i++) {
                    glm::vec4 vec4Value = attributeValues[i];
                    vec4AttributeValues.push_back(vec4Value); // maybe this is unecessary

                    // create importance criterion attributes
                    ImportanceCriterionAttribute importanceCriterionAttribute;
                    importanceCriterionAttribute.name = meshAttribute.name;
                }



//                attributeBuffer = Renderer->createGeometryBuffer(
//                        vec4AttributeValues.size() * sizeof(glm::vec4),
//                        (void*)&vec4AttributeValues.front(), bufferType);
            }

            if (!(useProgrammableFetch && programmableFetchUseAoS)
                && !(meshAttribute.numComponents == 1 && useProgrammableFetch)
                && !(meshAttribute.numComponents == 6 && useProgrammableFetch)
                && !(meshAttribute.numComponents == 2 && useProgrammableFetch)
                && !(meshAttribute.numComponents == 3 && useProgrammableFetch)) {
                attributeBuffer = Renderer->createGeometryBuffer(
                        meshAttribute.data.size(), (void*)&meshAttribute.data.front(), bufferType);
            }
            if (meshAttribute.numComponents == 3 && (useProgrammableFetch && !programmableFetchUseAoS)) {
                // vec3 problematic in std430 struct
                glm::vec3 *attributeValues = (glm::vec3*)&meshAttribute.data.front();
                size_t numAttributeValues = meshAttribute.data.size() / sizeof(glm::vec3);
                std::vector<glm::vec4> vec4AttributeValues;
                vec4AttributeValues.reserve(numAttributeValues);
                for (size_t i = 0; i < numAttributeValues; i++) {
                    glm::vec3 vec3Value = attributeValues[i];
                    vec4AttributeValues.push_back(glm::vec4(vec3Value.x, vec3Value.y, vec3Value.z, 1.0f));
                }
                attributeBuffer = Renderer->createGeometryBuffer(
                        vec4AttributeValues.size()*sizeof(glm::vec4), (void*)&vec4AttributeValues.front(), bufferType);
            }

            if (!useProgrammableFetch) {
                if (meshAttribute.numComponents == 1) {
                    // Importance criterion attributes are bound to location 3 and onwards in vertex shader
                    /*renderData->addGeometryBuffer(attributeBuffer, importanceCriterionLocationCounter,
                            meshAttribute.attributeFormat, meshAttribute.numComponents, 0, 0, 0, true); */

                    //! TODO This is the point where we set optional buffers
                    renderData->addGeometryBufferOptional(attributeBuffer, meshAttribute.name.c_str(),
                            meshAttribute.attributeFormat, meshAttribute.numComponents, 0, 0, 0, true);
                } else {
                    bool isNormalizedColor = (meshAttribute.name == "vertexColor");
                    renderData->addGeometryBufferOptional(attributeBuffer, meshAttribute.name.c_str(),
                            meshAttribute.attributeFormat, meshAttribute.numComponents, 0, 0, 0, isNormalizedColor);
                }
                meshRenderer.shaderAttributeNames.insert(meshAttribute.name);
            } else {
                if (programmableFetchUseAoS) {
                    if (meshAttribute.name == "vertexPosition") {
                        glm::vec3 *attributeValues = (glm::vec3*)&meshAttribute.data.front();
                        size_t numAttributeValues = meshAttribute.data.size() / sizeof(glm::vec3);
                        vertexPositionData.reserve(numAttributeValues);
                        for (size_t i = 0; i < numAttributeValues; i++) {
                            vertexPositionData.push_back(attributeValues[i]);
                        }
                    } else if (meshAttribute.name == "vertexLineTangent") {
                        glm::vec3 *attributeValues = (glm::vec3*)&meshAttribute.data.front();
                        size_t numAttributeValues = meshAttribute.data.size() / sizeof(glm::vec3);
                        vertexTangentData.reserve(numAttributeValues);
                        for (size_t i = 0; i < numAttributeValues; i++) {
                            vertexTangentData.push_back(attributeValues[i]);
                        }
                    }
                } else {
                    int bindingPoint = -1;
                    if (meshAttribute.name == "vertexPosition") {
                        bindingPoint = 2;
                    } else if (meshAttribute.name == "vertexLineTangent") {
                        bindingPoint = 3;
                    } else if (boost::starts_with(meshAttribute.name, "vertexAttribute")) {
                        bindingPoint = 4;
                    }
                    meshRenderer.ssboEntries.push_back(SSBOEntry(bindingPoint, meshAttribute.name, attributeBuffer));
                }
            }

            if (meshAttribute.name == "vertexPosition") {
                std::vector<glm::vec3> vertices;
                vertices.resize(meshAttribute.data.size() / sizeof(glm::vec3));
                memcpy(&vertices.front(), &meshAttribute.data.front(), meshAttribute.data.size());
                totalBoundingBox.combine(computeAABB(vertices));
            }
        }

        if (useProgrammableFetch && programmableFetchUseAoS) {
            for (size_t attributeIndex = 0; attributeIndex < vertexAttributeData.size(); attributeIndex++) {
                std::vector<LinePointData> linePointData;
                linePointData.resize(vertexPositionData.size());

                for (size_t i = 0; i < vertexPositionData.size(); i++) {
                    linePointData.at(i).vertexPosition = vertexPositionData.at(i);
                    linePointData.at(i).vertexAttribute = vertexAttributeData.at(attributeIndex).at(i);
                    linePointData.at(i).vertexTangent = vertexTangentData.at(i);
                    linePointData.at(i).padding = 0.0f;
                }

                GeometryBufferPtr attributeBuffer = Renderer->createGeometryBuffer(
                        linePointData.size()*sizeof(LinePointData), (void*)&linePointData.front(),
                        SHADER_STORAGE_BUFFER);
                meshRenderer.ssboEntries.push_back(SSBOEntry(2, "vertexAttribute" + sgl::toString(attributeIndex),
                        attributeBuffer));
            }
        }

        shaderAttributes.push_back(renderData);
        materials.push_back(mesh.submeshes.at(i).material);
    }

    meshRenderer.boundingBox = totalBoundingBox;
    meshRenderer.boundingSphere = sgl::Sphere(totalBoundingBox.getCenter(), glm::length(totalBoundingBox.getExtent()));

    return meshRenderer;
}
