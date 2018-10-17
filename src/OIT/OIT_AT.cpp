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

#include "OIT_AT.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_AT::OIT_AT()
{
    clearBitSet = true;
    create();
}

void OIT_AT::create()
{
    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeError("Error in OIT_KBuffer::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }

    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"ATGather.glsl\"");

    std::list<std::string> shaderIDs = {gatherShaderName + ".Vertex", gatherShaderName + ".Fragment"};
    if (gatherShaderName.find("Vorticity") != std::string::npos) {
        shaderIDs.push_back(gatherShaderName + ".Geometry");
    }
    gatherShader = ShaderManager->getShaderProgram(shaderIDs);
    resolveShader = ShaderManager->getShaderProgram({"ATResolve.Vertex", "ATResolve.Fragment"});
    clearShader = ShaderManager->getShaderProgram({"ATClear.Vertex", "ATClear.Fragment"});

    // Create blitting data (fullscreen rectangle in normalized device coordinates)
    blitRenderData = ShaderManager->createShaderAttributes(resolveShader);

    std::vector<glm::vec3> fullscreenQuad{
            glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
            glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
    GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
            (void*)&fullscreenQuad.front());
    blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

    clearRenderData = ShaderManager->createShaderAttributes(clearShader);
    geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
            (void*)&fullscreenQuad.front());
    clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
}

void OIT_AT::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
        sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    size_t bufferSize = width * height;
    size_t bufferSizeBytes = sizeof(ATFragmentNode_compressed) * bufferSize;
    void *data = (void*)malloc(bufferSizeBytes);
    memset(data, 0, bufferSizeBytes);

    fragmentNodes = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, data, SHADER_STORAGE_BUFFER);
    free(data);

    size_t numFragmentsBufferSizeBytes = sizeof(int32_t) * width * height;
    numFragmentsBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    numFragmentsBuffer = Renderer->createGeometryBuffer(numFragmentsBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

    gatherShader->setUniform("viewportW", width);
    gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    resolveShader->setUniform("viewportW", width);
    resolveShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    clearShader->setUniform("viewportW", width);
    clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

    // Buffer has to be cleared again
    clearBitSet = true;
}

void OIT_AT::gatherBegin()
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

    if (useStencilBuffer) {
        glEnable(GL_STENCIL_TEST);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
    }
}

void OIT_AT::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_AT::renderToScreen()
{
    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_STENCIL_TEST);

    if (useStencilBuffer) {
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilMask(0x00);
    }

    Renderer->render(blitRenderData);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
}
