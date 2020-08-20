/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2020, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "OIT_WBOIT.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>
#include <ImGui/ImGuiWrapper.hpp>

#include "OIT_WBOIT.hpp"
#include "BufferSizeWatch.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_WBOIT::OIT_WBOIT()
{
    create();
}

void OIT_WBOIT::create()
{
    ShaderManager->invalidateShaderCache();
    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"WBOITGather.glsl\"");
    reloadShaders();

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = ShaderManager->createShaderAttributes(resolveShader);

    std::vector<glm::vec3> fullscreenQuadPos{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    GeometryBufferPtr geomBufferPos = Renderer->createGeometryBuffer(
            sizeof(glm::vec3)*fullscreenQuadPos.size(), (void*)&fullscreenQuadPos.front());
    std::vector<glm::vec2> fullscreenQuadTex{
            glm::vec2(1,1), glm::vec2(0,0), glm::vec2(1,0),
            glm::vec2(0,0), glm::vec2(1,1), glm::vec2(0,1)};
    GeometryBufferPtr geomBufferTex = Renderer->createGeometryBuffer(
            sizeof(glm::vec2)*fullscreenQuadTex.size(), (void*)&fullscreenQuadTex.front());
    blitRenderData->addGeometryBuffer(geomBufferPos, "vertexPosition", ATTRIB_FLOAT, 3);
    blitRenderData->addGeometryBuffer(geomBufferTex, "vertexTexCoord", ATTRIB_FLOAT, 2);
}

void OIT_WBOIT::reloadShaders()
{
    ShaderManager->invalidateShaderCache();
    gatherShader = ShaderManager->getShaderProgram(gatherShaderIDs);
    resolveShader = ShaderManager->getShaderProgram({"WBOITResolve.Vertex", "WBOITResolve.Fragment"});

    if (blitRenderData) {
        blitRenderData = blitRenderData->copy(resolveShader);
    }
}

void OIT_WBOIT::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                                         sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    this->sceneFramebuffer = sceneFramebuffer;

    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    TextureSettings textureSettingsColor;
    textureSettingsColor.internalFormat = GL_RGBA32F; // GL_RGBA16F?
    TextureSettings textureSettingsDepth;
    textureSettingsDepth.internalFormat = GL_DEPTH_COMPONENT;
    textureSettingsDepth.pixelFormat = GL_DEPTH_COMPONENT;
    textureSettingsDepth.pixelType = GL_FLOAT;

    gatherPassFBO = Renderer->createFBO();
    accumulationRenderTexture = TextureManager->createEmptyTexture(width, height, textureSettingsColor);
    textureSettingsColor.internalFormat = GL_R32F; // GL_R16F?
    revealageRenderTexture = TextureManager->createEmptyTexture(width, height, textureSettingsColor);
    gatherPassFBO->bindTexture(accumulationRenderTexture, COLOR_ATTACHMENT0);
    gatherPassFBO->bindTexture(revealageRenderTexture, COLOR_ATTACHMENT1);
    gatherPassFBO->bindRenderbuffer(sceneDepthRBO, DEPTH_ATTACHMENT);
}

void OIT_WBOIT::setUniformData()
{
    resolveShader->setUniform("accumulationTexture", accumulationRenderTexture, 0);
    resolveShader->setUniform("revealageTexture", revealageRenderTexture, 1);
}

void OIT_WBOIT::gatherBegin()
{
    setUniformData();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    sgl::Renderer->bindFBO(gatherPassFBO);
    const float rgba32Zero[] = { 0, 0, 0, 0 };
    glClearBufferfv(GL_COLOR, 0, rgba32Zero);
    const float r32One[] = { 1, 0, 0, 0 };
    glClearBufferfv(GL_COLOR, 1, r32One);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
}

void OIT_WBOIT::renderGUI()
{
    //ImGui::Separator();
    //ImGui::Text("Max. depth complexity: %lu", maxDepthComplexity);
}


void OIT_WBOIT::renderScene()
{
    renderSceneFunction();
}

void OIT_WBOIT::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_WBOIT::renderToScreen()
{
    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    Renderer->bindFBO(sceneFramebuffer);
    // Normal alpha blending
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    Renderer->render(blitRenderData);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

