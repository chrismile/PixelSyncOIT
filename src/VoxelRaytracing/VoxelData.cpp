//
// Created by christoph on 04.10.18.
//

#include <cstring>
#include <fstream>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <Utils/Events/Stream/Stream.hpp>
#include <Utils/File/Logfile.hpp>
#include <Utils/Convert.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/OpenGL/Texture.hpp>

#include "VoxelData.hpp"

const uint32_t VOXEL_GRID_FORMAT_VERSION = 1u;

void saveToFile(const std::string &filename, const VoxelGridDataCompressed &data)
{
    std::ofstream file(filename.c_str(), std::ofstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in saveToFile: File \"" + filename + "\" not found.");
        return;
    }

    sgl::BinaryWriteStream stream;
    stream.write((uint32_t)VOXEL_GRID_FORMAT_VERSION);
    stream.write(data.gridResolution);
    stream.write(data.quantizationResolution);
    stream.write(data.worldToVoxelGridMatrix);

    stream.writeArray(data.voxelLineListOffsets);
    stream.writeArray(data.numLinesInVoxel);
    stream.writeArray(data.voxelDensityLODs);
    stream.writeArray(data.lineSegments);

    file.write((const char*)stream.getBuffer(), stream.getSize());
    file.close();
}

void loadFromFile(const std::string &filename, VoxelGridDataCompressed &data)
{
    std::ifstream file(filename.c_str(), std::ifstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in loadFromFile: File \"" + filename + "\" not found.");
        return;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0);
    char *buffer = new char[size];
    file.read(buffer, size);

    sgl::BinaryReadStream stream(buffer, size);
    uint32_t version;
    stream.read(version);
    if (version != VOXEL_GRID_FORMAT_VERSION) {
        sgl::Logfile::get()->writeError(std::string() + "Error in loadFromFile: Invalid version in file \""
                                        + filename + "\".");
        return;
    }

    stream.read(data.gridResolution);
    stream.read(data.quantizationResolution);
    stream.read(data.worldToVoxelGridMatrix);

    stream.readArray(data.voxelLineListOffsets);
    stream.readArray(data.numLinesInVoxel);
    stream.readArray(data.voxelDensityLODs);
    stream.readArray(data.lineSegments);

    //delete[] buffer; // BinaryReadStream does deallocation
    file.close();
}




std::vector<float> generateMipmapsForDensity(float *density, glm::ivec3 size)
{
    std::vector<float> allLODs;
    size_t memorySize = 0;
    for (glm::ivec3 lodSize = size; lodSize.x > 0 && lodSize.y > 0 && lodSize.z > 0; lodSize /= 2) {
        memorySize += lodSize.x * lodSize.y * lodSize.z;
    }
    allLODs.reserve(memorySize);

    for (int i = 0; i < size.x * size.y * size.z; i++) {
        allLODs.push_back(density[i]);
    }

    float *lodData = new float[size.x * size.y * size.z];
    float *lodDataLast = new float[size.x * size.y * size.z];
    memcpy(lodDataLast, density, size.x * size.y * size.z * sizeof(float));
    for (glm::ivec3 lodSize = size/2; lodSize.x > 0 && lodSize.y > 0 && lodSize.z > 0; lodSize /= 2) {
        // Averaging operation
        for (int i = 0; i < lodSize.x * lodSize.y * lodSize.z; i++) {
            lodData[i] = 0.0f;
            for (int j = 0; j < 8; j++) {
                lodData[i] += lodDataLast[i*8+j];
            }
            lodData[i] = (lodDataLast[i*8]+lodDataLast[i*8+1]
                          + lodDataLast[i*8 + lodSize.x*2]+lodDataLast[i*8+1 + lodSize.x*2]
                          + lodDataLast[i*8 + lodSize.x*lodSize.y*4]+lodDataLast[i*8+1 + lodSize.x*lodSize.y*4]
                          + lodDataLast[i*8 + lodSize.x*2 + lodSize.x*lodSize.y*4]+lodDataLast[i*8+1 + lodSize.x*2 + lodSize.x*lodSize.y*4]) / 8.0f;
            lodData[i] /= 8.0f;
            allLODs.push_back(lodData[i]);
        }
    }

    delete[] lodData;
    delete[] lodDataLast;
    return allLODs;
}

sgl::TexturePtr generateDensityTexture(const std::vector<float> &lods, glm::ivec3 size)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_3D, textureID);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#ifdef USE_OPENGL_LOD_GENERATION
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, size.x, size.y, size.z, 0, GL_RED, GL_FLOAT, &lods.front());
    glGenerateMipmap(GL_TEXTURE_3D);
#else
    // Now upload the LOD levels
    int lodIndex = 0;
    const float *data = &lods.front();
    for (glm::ivec3 lodSize = size; lodSize.x > 0 && lodSize.y > 0 && lodSize.z > 0; lodSize /= 2) {
        glTexImage3D(GL_TEXTURE_3D, lodIndex, GL_R32F, lodSize.x, lodSize.y, lodSize.z, 0, GL_RED, GL_FLOAT, data);
        lodIndex++;
        data += lodSize.x * lodSize.y * lodSize.z;
    }
#endif

    sgl::TextureSettings textureSettings;
    textureSettings.type = sgl::TEXTURE_3D;
    return sgl::TexturePtr(new sgl::TextureGL(textureID, size.x, size.y, size.z, textureSettings));
}

void compressedToGPUData(const VoxelGridDataCompressed &compressedData, VoxelGridDataGPU &gpuData)
{
    gpuData.gridResolution = compressedData.gridResolution;
    gpuData.quantizationResolution = compressedData.quantizationResolution;
    gpuData.worldToVoxelGridMatrix = compressedData.worldToVoxelGridMatrix;

    gpuData.voxelLineListOffsets = sgl::Renderer->createGeometryBuffer(
            sizeof(uint32_t)*compressedData.voxelLineListOffsets.size(),
            (void*)&compressedData.voxelLineListOffsets.front());
    gpuData.numLinesInVoxel = sgl::Renderer->createGeometryBuffer(
            sizeof(uint32_t)*compressedData.numLinesInVoxel.size(),
            (void*)&compressedData.numLinesInVoxel.front());

    gpuData.densityTexture = generateDensityTexture(compressedData.voxelDensityLODs, gpuData.gridResolution);

    // TODO: Format (NOT uint32_t)
    gpuData.lineSegments = sgl::Renderer->createGeometryBuffer(
            sizeof(uint32_t)*compressedData.lineSegments.size(),
            (void*)&compressedData.lineSegments.front());
}