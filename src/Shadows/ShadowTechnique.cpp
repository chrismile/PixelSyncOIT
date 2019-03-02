//
// Created by christoph on 10.02.19.
//

#include <boost/algorithm/string/predicate.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include "ShadowTechnique.hpp"

void ShadowTechnique::setLightDirection(const glm::vec3 &lightDirection, const sgl::AABB3 &sceneBoundingBox)
{
    glm::vec3 cameraFront = lightDirection; // Gram-Schmidt
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // Gram-Schmidt
    cameraUp = glm::normalize(cameraUp - cameraFront * glm::dot(cameraUp, cameraFront)); // Gram-Schmidt
    glm::mat4 boundingBoxRotationMatrix = glm::lookAt(glm::vec3(0.0f), cameraFront, cameraUp);

    glm::vec3 sceneCenter = sceneBoundingBox.getCenter();
    sgl::AABB3 lightFacingBoundingBox = sceneBoundingBox.transformed(boundingBoxRotationMatrix);
    float screenHalfWidth = std::max(lightFacingBoundingBox.getExtent().x, lightFacingBoundingBox.getExtent().y)*1.0f;
    //std::cout << "sceneCenter: (" << sceneCenter.x << ", " << sceneCenter.y << ", " << sceneCenter.z << ")" << std::endl;
    lightProjectionMatrix = glm::ortho(-screenHalfWidth, screenHalfWidth, -screenHalfWidth, screenHalfWidth,
            LIGHT_NEAR_CLIP_DISTANCE, LIGHT_FAR_CLIP_DISTANCE);
    lightViewMatrix = glm::lookAt(lightDirection+sceneCenter, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f))
            *sgl::matrixTranslation(-2.0f*lightDirection);
    lightSpaceMatrix = lightProjectionMatrix * lightViewMatrix;
    this->lightDirection = lightDirection;
}

void ShadowTechnique::newModelLoaded(const std::string &filename, bool modelContainsTrajectories)
{
    if (modelContainsTrajectories) {
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
