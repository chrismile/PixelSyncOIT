//
// Created by christoph on 10.02.19.
//

#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Texture/Bitmap.hpp>
#include <Graphics/OpenGL/Texture.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/ShaderAttributes.hpp>
#include <Utils/AppSettings.hpp>
#include <ImGui/ImGuiWrapper.hpp>
#include "ShadowMapping.hpp"

static int SHADOW_MAP_RESOLUTION = 2048;

ShadowMapping::ShadowMapping()
{
    loadShaders();
    resolutionChanged();
}

void ShadowMapping::loadShaders()
{
}

void ShadowMapping::setGatherShaderList(const std::list<std::string> &shaderIDs)
{
    gatherShaderIDs = shaderIDs;
    sgl::ShaderManager->invalidateShaderCache();
    std::string oldGatherHeader = sgl::ShaderManager->getPreprocessorDefine("OIT_GATHER_HEADER");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"GenerateShadowMap.glsl\"");
    sgl::ShaderManager->addPreprocessorDefine("SHADOW_MAP_GENERATE", "");
    createShadowMapShader = sgl::ShaderManager->getShaderProgram(gatherShaderIDs);
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAP_GENERATE");
    sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", oldGatherHeader);
}

void ShadowMapping::createShadowMapPass(std::function<void()> sceneRenderFunction)
{
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    sgl::Renderer->bindFBO(shadowMapFBO);
    sgl::Renderer->setViewMatrix(lightViewMatrix);
    sgl::Renderer->setProjectionMatrix(lightProjectionMatrix);
    glViewport(0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION);
    glClear(GL_DEPTH_BUFFER_BIT);
    preRenderPass = true;
    sceneRenderFunction();
    preRenderPass = false;

    //Renderer->blurTexture(ssaoTexture);
    sgl::Renderer->unbindFBO();
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}

void ShadowMapping::setUniformValuesCreateShadowMap()
{
}

void ShadowMapping::setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader)
{
    transparencyShader->setUniform("lightSpaceMatrix", lightSpaceMatrix);
    transparencyShader->setUniform("shadowMap", shadowMap, 7);
}

void ShadowMapping::setShaderDefines()
{
    sgl::ShaderManager->addPreprocessorDefine("SHADOW_MAPPING_STANDARD", "");
    sgl::ShaderManager->removePreprocessorDefine("SHADOW_MAPPING_MOMENTS");
    sgl::ShaderManager->invalidateShaderCache();
}

void ShadowMapping::resolutionChanged()
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    shadowMapFBO = sgl::Renderer->createFBO();

    sgl::TextureSettings textureSettings;
    textureSettings.internalFormat = GL_DEPTH_COMPONENT;
    textureSettings.pixelFormat = GL_DEPTH_COMPONENT;
    textureSettings.pixelType = GL_FLOAT;
    shadowMap = sgl::TextureManager->createEmptyTexture(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, textureSettings);
    shadowMapFBO->bindTexture(shadowMap, sgl::DEPTH_ATTACHMENT);
}

bool ShadowMapping::renderGUI()
{
    bool reRender = false;
    if (ImGui::SliderInt("", &SHADOW_MAP_RESOLUTION, 256, 4096)) {
        reRender = true;
        resolutionChanged();
    }
    return reRender;
}
