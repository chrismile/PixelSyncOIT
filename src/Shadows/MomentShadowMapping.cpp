//
// Created by christoph on 10.02.19.
//

#include <Graphics/Shader/ShaderManager.hpp>
#include "MomentShadowMapping.hpp"

MomentShadowMapping::MomentShadowMapping()
{
    loadShaders();
    resolutionChanged();
}

void MomentShadowMapping::loadShaders()
{
    createShadowMapShader = sgl::ShaderManager->getShaderProgram(
            {"GenerateMomentShadowMap.Vertex", "GenerateMomentShadowMap.Fragment"});
    clearShadowMapShader = sgl::ShaderManager->getShaderProgram(
            {"ClearMomentShadowMap.Vertex", "ClearMomentShadowMap.Fragment"});
}

void MomentShadowMapping::createShadowMapPass(std::function<void()> sceneRenderFunction)
{
    ;
}

void MomentShadowMapping::setUniformValuesCreateShadowMap()
{
    ;
}

void MomentShadowMapping::setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader)
{
    ;
}

void MomentShadowMapping::setShaderDefines()
{
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAPPING_STANDARD");
    sgl::ShaderManager->addPreprocessorDefine("SHADOW_MAPPING_MOMENTS", "");
    sgl::ShaderManager->invalidateShaderCache();
}

void MomentShadowMapping::resolutionChanged()
{
    ;
}

void MomentShadowMapping::renderGUI()
{
    // TODO
}
