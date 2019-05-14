//
// Created by christoph on 09.02.19.
//

#include <chrono>
#include <boost/algorithm/string/predicate.hpp>

#include <Utils/File/FileUtils.hpp>
#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>

#include "../VoxelRaytracing/VoxelData.hpp"
#include "../VoxelRaytracing/VoxelCurveDiscretizer.hpp"

#include "VoxelAO.hpp"

void VoxelAOHelper::loadAOFactorsFromVoxelFile(const std::string &filename)
{
    // Check if voxel grid is already created
    // Pure filename without extension (to create compressed .voxel filename)
    std::string modelFilenamePure = sgl::FileUtils::get()->removeExtension(filename);

    std::string modelFilenameVoxelGrid = modelFilenamePure + ".voxel";

    // Can be either hair dataset or trajectory dataset
    bool isHairDataset = boost::starts_with(modelFilenamePure, "Data/Hair");
    bool isRings = boost::starts_with(modelFilenamePure, "Data/Rings");
    bool isConvectionRolls = boost::starts_with(modelFilenamePure, "Data/ConvectionRolls");
    bool isAneurysm = boost::starts_with(modelFilenamePure, "Data/Trajectories");

    auto start = std::chrono::system_clock::now();

    uint16_t voxelRes = 128;
    if (isRings) {
        voxelRes = 32;
    }
    if (isAneurysm)
    {
        voxelRes = 128;
    }

    VoxelGridDataCompressed compressedData;
    if (!sgl::FileUtils::get()->exists(modelFilenameVoxelGrid)) {
        VoxelCurveDiscretizer discretizer(glm::ivec3(voxelRes), glm::ivec3(64));

        int maxNumLinesPerVoxel = 32;
        if (boost::starts_with(filename, "Data/WCB")) {
            maxNumLinesPerVoxel = 128;
        } else if (boost::starts_with(filename, "Data/ConvectionRolls/turbulence20000")){
            maxNumLinesPerVoxel = 64;
        }

        if (isHairDataset) {
            std::string modelFilenameHair = modelFilenamePure + ".hair";
            float lineRadius;
            glm::vec4 hairStrandColor;
            compressedData = discretizer.createFromHairDataset(modelFilenameHair, lineRadius, hairStrandColor,
                    maxNumLinesPerVoxel);
        } else {
            std::string modelFilenameObj = modelFilenamePure + ".obj";
            std::vector<float> attributes;
            float maxVorticity;
            compressedData = discretizer.createFromTrajectoryDataset(modelFilenameObj, attributes, maxVorticity,
                    maxNumLinesPerVoxel);
        }

        auto end = std::chrono::system_clock::now();
        auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        sgl::Logfile::get()->writeInfo(std::string() + "Computational time to create voxel density for AO: "
                                       + std::to_string(elapsed.count()));

        saveToFile(modelFilenameVoxelGrid, compressedData);
    } else {
        loadFromFile(modelFilenameVoxelGrid, compressedData);
    }

    aoTexture = generateDensityTexture(compressedData.voxelAOLODs, compressedData.gridResolution);
    worldToVoxelGridMatrix = compressedData.worldToVoxelGridMatrix;
    gridResolution = compressedData.gridResolution;
}

void VoxelAOHelper::setUniformValues(sgl::ShaderProgramPtr transparencyShader)
{
    sgl::TexturePtr aoTexture = getAOTexture();
    transparencyShader->setUniform("aoTexture", aoTexture, 4);
    glm::mat4 worldSpaceToVoxelTextureSpace =
            sgl::matrixScaling(1.0f / glm::vec3(gridResolution)) * worldToVoxelGridMatrix;
    transparencyShader->setUniform("worldSpaceToVoxelTextureSpace", worldSpaceToVoxelTextureSpace);
}
