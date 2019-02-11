//
// Created by christoph on 10.02.19.
//

#include <boost/algorithm/string/predicate.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include "ShadowTechnique.hpp"

void ShadowTechnique::setLightDirection(const glm::vec3 &lightDirection, const glm::vec3 &sceneCenter)
{
    float near_plane = -2.0f, far_plane = 2.0f;
    lightProjectionMatrix = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, near_plane, far_plane);
    lightViewMatrix = glm::lookAt(lightDirection+sceneCenter, glm::vec3(0.0f, 0.0f,  0.0f)+sceneCenter, glm::vec3(0.0f, 1.0f,  0.0f));
    lightSpaceMatrix = lightProjectionMatrix * lightViewMatrix;
    this->lightDirection = lightDirection;
}

void  ShadowTechnique::newModelLoaded(const std::string &filename)
{
    if (boost::starts_with(filename, "Data/Trajectories")) {
        sgl::ShaderManager->addPreprocessorDefine("MODEL_WITH_VORTICITY", "");
    } else {
        sgl::ShaderManager->removePreprocessorDefine("MODEL_WITH_VORTICITY");
    }
    sgl::ShaderManager->invalidateShaderCache();
    loadShaders();
}

void NoShadowMapping::setShaderDefines()
{
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAPPING_STANDARD");
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAPPING_MOMENTS");
    sgl::ShaderManager->invalidateShaderCache();
}
