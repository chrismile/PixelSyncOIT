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
#include <ImGui/ImGuiWrapper.hpp>

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
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"DepthPeelingGather.glsl\"");
    std::list<std::string> shaderIDs = {gatherShaderName + ".Vertex", gatherShaderName + ".Fragment"};
    if (gatherShaderName.find("Vorticity") != std::string::npos) {
        shaderIDs.push_back(gatherShaderName + ".Geometry");
    }
    gatherShader = ShaderManager->getShaderProgram(shaderIDs);

    // Create render data for determining depth complexity.
    //depthComplexityGatherShader = ShaderManager->getShaderProgram({"DepthPeelingGatherDepthComplexity.Vertex",
    //                                                               "DepthPeelingGatherDepthComplexity.Fragment"});
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"DepthComplexityGather.glsl\"");
    shaderIDs = {gatherShaderName + ".Vertex", gatherShaderName + ".Fragment"};
    if (gatherShaderName.find("Vorticity") != std::string::npos) {
        shaderIDs.push_back(gatherShaderName + ".Geometry");
    }
    depthComplexityGatherShader = ShaderManager->getShaderProgram(shaderIDs);
}

void OIT_DepthPeeling::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
        sgl::RenderbufferObjectPtr &sceneDepthRBO)
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



    // Buffer for determining the (maximum) depth complexity of the scene
    size_t numFragmentsBufferSizeBytes = sizeof(uint32_t) * width * height;
    numFragmentsBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    numFragmentsBuffer = Renderer->createGeometryBuffer(numFragmentsBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);
}

void OIT_DepthPeeling::setUniformData()
{
    depthComplexityGatherShader->setUniform("viewportW", sgl::AppSettings::get()->getMainWindow()->getWidth());
    depthComplexityGatherShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);
}

void OIT_DepthPeeling::gatherBegin()
{
    setUniformData();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    computeDepthComplexity();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE); // Front-to-back blending


}

void OIT_DepthPeeling::computeDepthComplexity()
{
    // Clear numFragmentsBuffer
    GLuint bufferID = static_cast<GeometryBufferGL*>(numFragmentsBuffer.get())->getBuffer();
    GLubyte val = 0;
    glClearNamedBufferData(bufferID, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, (const void*)&val);

    // Render to numFragmentsBuffer to determine the depth complexity of the scene
    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    sgl::ShaderProgramPtr gatherShaderTmp = gatherShader;
    gatherShader = depthComplexityGatherShader;
    renderSceneFunction();
    gatherShader = gatherShaderTmp;
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Compute the maximum depth complexity of the scene
    Window *window = AppSettings::get()->getMainWindow();
    int bufferSize = window->getWidth() * window->getHeight();
    uint32_t *data = (uint32_t*)numFragmentsBuffer->mapBuffer(BUFFER_MAP_READ_ONLY);

    int maxDepthComplexity = 0;
    #pragma omp parallel for reduction(max:maxDepthComplexity) schedule(static)
    for (int i = 0; i < bufferSize; i++) {
        maxDepthComplexity = std::max(maxDepthComplexity, (int)data[i]);
    }
    this->maxDepthComplexity = maxDepthComplexity;

    numFragmentsBuffer->unmapBuffer();
}

void OIT_DepthPeeling::renderGUI()
{
    ImGui::Separator();
    ImGui::Text("Max. depth complexity: %d", maxDepthComplexity);
}


void OIT_DepthPeeling::renderScene()
{
    for (int i = 0; i < 2; i++) {
        Renderer->bindFBO(depthPeelingFBOs[i]);
        Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, Color(0, 0, 0, 0), 1.0f);
    }

    Renderer->bindFBO(accumulatorFBO);
    Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT, Color(0, 0, 0, 0));

    for (int i = 0; i < maxDepthComplexity; i++) {
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

        // NVIDIA OpenGL Linux driver assumes the application has hung if we don't swap buffers every now and then
        if (i % 200 == 0 && i > 0) {
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
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Pre-multiplied alpha
    Renderer->blitTexture(colorAccumulatorTexture, AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)));

    // Revert to normal alpha blending
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

