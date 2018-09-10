//
// Created by christoph on 02.09.18.
//

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "OIT_HT.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_HT::OIT_HT()
{
    clearBitSet = true;
    create();
}

void OIT_HT::create()
{
    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeError("Error in OIT_PixelSync::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }

    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"HTGather.glsl\"");

    gatherShader = ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
    blitShader = ShaderManager->getShaderProgram({"HTResolve.Vertex", "HTResolve.Fragment"});
    clearShader = ShaderManager->getShaderProgram({"HTClear.Vertex", "HTClear.Fragment"});

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = ShaderManager->createShaderAttributes(blitShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
    blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

    clearRenderData = ShaderManager->createShaderAttributes(clearShader);
    geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
    clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
}

void OIT_HT::resolutionChanged()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    size_t bufferSizeBytes = sizeof(HTFragmentNode_compressed) * width * height;
    void *data = (void*)malloc(bufferSizeBytes);
    memset(data, 0, bufferSizeBytes);

    fragmentNodes = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, data, SHADER_STORAGE_BUFFER);
    free(data);

    size_t fragmentTailsSizeBytes = sizeof(HTFragmentTail_compressed) * width * height;
    fragmentTails = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    fragmentTails = Renderer->createGeometryBuffer(fragmentTailsSizeBytes, NULL, SHADER_STORAGE_BUFFER);

    gatherShader->setUniform("viewportW", width);
    gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
    gatherShader->setShaderStorageBuffer(1, "FragmentTails", fragmentTails);

    blitShader->setUniform("viewportW", width);
    blitShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
    blitShader->setShaderStorageBuffer(1, "FragmentTails", fragmentTails);

    clearShader->setUniform("viewportW", width);
    clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
    clearShader->setShaderStorageBuffer(1, "FragmentTails", fragmentTails);

    // Buffer has to be cleared again
    clearBitSet = true;
}

void OIT_HT::gatherBegin()
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());
    if (clearBitSet) {
        Renderer->render(clearRenderData);
        clearBitSet = false;
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LESS);
    //glDepthMask(GL_FALSE);

    if (useStencilBuffer) {
        glEnable(GL_STENCIL_TEST);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
    }
}

void OIT_HT::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_HT::renderToScreen()
{
    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    if (useStencilBuffer) {
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilMask(0x00);
    }

    Renderer->render(blitRenderData);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glDepthMask(GL_TRUE);
}
