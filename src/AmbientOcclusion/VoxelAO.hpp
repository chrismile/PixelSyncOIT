//
// Created by christoph on 09.02.19.
//

#ifndef PIXELSYNCOIT_VOXELAO_H
#define PIXELSYNCOIT_VOXELAO_H

#include <string>
#include <Graphics/Shader/Shader.hpp>
#include "Utils/ImportanceCriteria.hpp"

class VoxelAOHelper
{
public:
    void loadAOFactorsFromVoxelFile(const std::string &filename, TrajectoryType trajectoryType);
    void setUniformValues(sgl::ShaderProgramPtr transparencyShader);
    inline sgl::TexturePtr getAOTexture() { return aoTexture; }

private:
    /// A 3D texture containing the ambient occlusion factors in the range [0,1]
    sgl::TexturePtr aoTexture;
    glm::mat4 worldToVoxelGridMatrix;
    glm::ivec3 gridResolution;
};


#endif //PIXELSYNCOIT_VOXELAO_H
