/*
 * OIT_DepthComplexity.cpp
 *
 *  Created on: 23.08.2018
 *      Author: Christoph Neuhauser
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>
#include <Utils/Timer.hpp>

#include "OIT_DepthComplexity.hpp"

using namespace sgl;

// Number of transparent pixels we can store per node
const int nodesPerPixel = 8;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_DepthComplexity::OIT_DepthComplexity()
{
    create();
}

void OIT_DepthComplexity::create()
{
    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeError("Error in OIT_PixelSync::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }
    numFragmentsMaxColor = 16;

    gatherShader = ShaderManager->getShaderProgram({"DepthComplexityGather.Vertex", "DepthComplexityGather.Fragment"});

    blitShader = ShaderManager->getShaderProgram({"DepthComplexityResolve.Vertex", "DepthComplexityResolve.Fragment"});
    blitShader->setUniform("color", Color(0, 255, 255));
    blitShader->setUniform("numFragmentsMaxColor", numFragmentsMaxColor);

    clearShader = ShaderManager->getShaderProgram({"DepthComplexityClear.Vertex", "DepthComplexityClear.Fragment"});

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

    //Renderer->errorCheck();
}

void OIT_DepthComplexity::resolutionChanged()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    size_t numFragmentsBufferSizeBytes = sizeof(uint32_t) * width * height;
    numFragmentsBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    numFragmentsBuffer = Renderer->createGeometryBuffer(numFragmentsBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

    gatherShader->setUniform("viewportW", width);
    //gatherShader->setUniform("viewportH", height); // Not needed
    gatherShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

    blitShader->setUniform("viewportW", width);
    //blitShader->setUniform("viewportH", height); // Not needed
    blitShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

    clearShader->setUniform("viewportW", width);
    //clearShader->setUniform("viewportH", height); // Not needed
    clearShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);
}

void OIT_DepthComplexity::gatherBegin()
{
    //glClearDepth(0.0);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());
    //glEnable(GL_RASTERIZER_DISCARD);
    Renderer->render(clearRenderData);
    /*GLuint bufferID = static_cast<GeometryBufferGL*>(fragmentNodes.get())->getBuffer();
    GLubyte val = 0;
    glClearNamedBufferData(bufferID, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, (const void*)&val);*/
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LESS);
    //glDepthMask(GL_FALSE);

    if (useStencilBuffer) {
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
    }
}

void OIT_DepthComplexity::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Print statistics if enough time has passed
    static float counterPrintFrags = 0.0f;
    counterPrintFrags += Timer->getElapsed();
    if (counterPrintFrags > 1.0f) {
        computeStatistics();
        counterPrintFrags = 0.0f;
    }
}

void OIT_DepthComplexity::renderToScreen()
{
    Renderer->setProjectionMatrix(matrixIdentity());
    Renderer->setViewMatrix(matrixIdentity());
    Renderer->setModelMatrix(matrixIdentity());

    //glDisable(GL_RASTERIZER_DISCARD);

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

void OIT_DepthComplexity::computeStatistics()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    int size = width * height;

    uint32_t *data = (uint32_t*)numFragmentsBuffer->mapBuffer(BUFFER_MAP_READ_ONLY);

    int totalNumFragments = 0;
    int usedLocations = 0;
    int maxComplexity = 0;
    #pragma omp parallel for reduction(+:totalNumFragments,usedLocations) reduction(max:maxComplexity) schedule(static)
    for (int i = 0; i < size; i++) {
        totalNumFragments += data[i];
        if (data[i] > 0) {
            usedLocations++;
        }
        maxComplexity = std::max(maxComplexity, (int)data[i]);
    }

    numFragmentsBuffer->unmapBuffer();

    if ((uint32_t)maxComplexity > numFragmentsMaxColor) {
        numFragmentsMaxColor = maxComplexity;
        blitShader->setUniform("numFragmentsMaxColor", numFragmentsMaxColor);
    }

    if (totalNumFragments == 0) usedLocations = 1; // Avoid dividing by zero in code below
    std::cout << "Depth complexity: avg used: " << ((float) totalNumFragments / usedLocations)
              << ", avg all: " << ((float) totalNumFragments / size) << ", max:" << maxComplexity
              << ", #fragments: " << totalNumFragments << std::endl;
}
