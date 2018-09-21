//
// Created by christoph on 21.09.18.
//

#include "OIT_DepthPeeling.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "OIT_DepthPeeling.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_DepthPeeling::OIT_DepthPeeling()
{
    create();
}

void OIT_DepthPeeling::create()
{
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"DepthPeelingGather.glsl\"");
    gatherShader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
}

void OIT_DepthPeeling::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    this->sceneFramebuffer = sceneFramebuffer;

    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();



    TextureSettings textureSettingsColor;
    textureSettingsColor.internalFormat = GL_RGBA32F;
    TextureSettings textureSettingsDepth;
    textureSettingsDepth.internalFormat = GL_DEPTH_COMPONENT;
    textureSettingsDepth.pixelFormat = GL_DEPTH_COMPONENT;
    textureSettingsDepth.pixelType = GL_FLOAT;

    accumulatorFBO = Renderer->createFBO();
    colorAccumulatorTexture = TextureManager->createEmptyTexture(width, height, textureSettingsColor);
    accumulatorFBO->bindTexture(colorAccumulatorTexture, COLOR_ATTACHMENT);

    for (int i = 0; i < 2; i++) {
        depthPeelingFBOs[i] = Renderer->createFBO();

        colorRenderTextures[i] = TextureManager->createEmptyTexture(width, height, textureSettingsColor);
        depthPeelingFBOs[i]->bindTexture(colorRenderTextures[i], COLOR_ATTACHMENT);

        depthRenderTextures[i] = TextureManager->createEmptyTexture(width, height, textureSettingsDepth);
        depthPeelingFBOs[i]->bindTexture(depthRenderTextures[i], DEPTH_ATTACHMENT);
    }
}

void OIT_DepthPeeling::setUniformData()
{
}

void OIT_DepthPeeling::gatherBegin()
{
    setUniformData();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE); // Front-to-back blending
}

void OIT_DepthPeeling::renderScene()
{
    for (int i = 0; i < 2; i++) {
        Renderer->bindFBO(depthPeelingFBOs[i]);
        Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, Color(0, 0, 0, 0), 1.0f);
    }

    Renderer->bindFBO(accumulatorFBO);
    Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT, Color(0, 0, 0, 0));

    const int depthComplexity = 3000;

    for (int i = 0; i < depthComplexity; i++) {
        // 1. Peel one layer of the scene
        glDisable(GL_BLEND); // Replace with current surface
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        Renderer->bindFBO(depthPeelingFBOs[i%2]);
        Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, Color(0, 0, 0, 0), 1.0f);
        gatherShader->setUniform("depthReadTexture", depthRenderTextures[(i+1)%2], 0);
        gatherShader->setUniform("iteration", i);
        renderSceneFunction();

        // 2. Store it in the accumulator
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        Renderer->bindFBO(accumulatorFBO);
        Renderer->setProjectionMatrix(matrixIdentity());
        Renderer->setViewMatrix(matrixIdentity());
        Renderer->setModelMatrix(matrixIdentity());
        Renderer->blitTexture(colorRenderTextures[i%2], AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)));

        if (i % 500 == 0) {
            sgl::AppSettings::get()->getMainWindow()->flip();
        }
    }
}

void OIT_DepthPeeling::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_DepthPeeling::renderToScreen()
{
    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    Renderer->bindFBO(sceneFramebuffer);
    Renderer->clearFramebuffer();
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Pre-multiplied alpha
    Renderer->blitTexture(colorAccumulatorTexture, AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)));

    // Revert to normal alpha blending
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}
