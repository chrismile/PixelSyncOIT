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
#include <ImGui/ImGuiWrapper.hpp>

#include "../Performance/AutoPerfMeasurer.hpp"
#include "OIT_DepthComplexity.hpp"
#include "BufferSizeWatch.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_DepthComplexity::OIT_DepthComplexity()
{
    create();
}

void OIT_DepthComplexity::create()
{
    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeError("Error in OIT_KBuffer::create: GL_ARB_fragment_shader_interlock unsupported.");
        exit(1);
    }
    numFragmentsMaxColor = 256;

    ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"DepthComplexityGather.glsl\"");
    gatherShader = ShaderManager->getShaderProgram(gatherShaderIDs);

    resolveShader = ShaderManager->getShaderProgram({"DepthComplexityResolve.Vertex", "DepthComplexityResolve.Fragment"});
    resolveShader->setUniform("color", Color(0, 255, 255));
    resolveShader->setUniform("numFragmentsMaxColor", numFragmentsMaxColor);

    clearShader = ShaderManager->getShaderProgram({"DepthComplexityClear.Vertex", "DepthComplexityClear.Fragment"});

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

    //Renderer->errorCheck();
}

void OIT_DepthComplexity::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
        sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    size_t numFragmentsBufferSizeBytes = sizeof(uint32_t) * width * height;
    numFragmentsBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
    numFragmentsBuffer = Renderer->createGeometryBuffer(numFragmentsBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

    setCurrentAlgorithmBufferSizeBytes(numFragmentsBufferSizeBytes);
}

void OIT_DepthComplexity::setUniformData()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    gatherShader->setUniform("viewportW", width);
    gatherShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

    resolveShader->setUniform("viewportW", width);
    resolveShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

    clearShader->setUniform("viewportW", width);
    clearShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);
}

void OIT_DepthComplexity::gatherBegin()
{
    setUniformData();

    resolveShader->setUniform("numFragmentsMaxColor", numFragmentsMaxColor);

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

    if (useStencilBuffer) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
        glClear(GL_STENCIL_BUFFER_BIT);
    }
}

void OIT_DepthComplexity::gatherEnd()
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    if (performanceMeasureMode || recordingMode) {
        firstFrame = false;
        computeStatistics();
    }
}

void OIT_DepthComplexity::renderToScreen()
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

    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
}



// Converts e.g. 123456789 to "123,456,789"
std::string numberToCommaString(int number, bool attachLeadingZeroes = false) {
    if (number < 0) {
        return std::string() + "-" + numberToCommaString(-number, attachLeadingZeroes);
    } else if (number < 1000) {
        return toString(number);
    } else {
        std::string numberString = toString(number%1000);
        while (attachLeadingZeroes && numberString.size() < 3) {
            numberString = "0" + numberString;
        }
        return std::string() + numberToCommaString(number/1000, true) + "," + numberString;
    }
}

static float intensity = 1.0f;
void OIT_DepthComplexity::renderGUI()
{
    ImGui::Separator();

    std::string totalNumFragmentsString = numberToCommaString(totalNumFragments);
    ImGui::Text("Depth complexity: #fragments: %s", totalNumFragmentsString.c_str());
    ImGui::Text("avg used: %.2f, avg all: %.2f, max: %d", ((float) totalNumFragments / usedLocations),
                ((float) totalNumFragments / bufferSize), maxComplexity);

    static ImVec4 colorSelection = ImColor(0, 255, 255, 127);
    if (ImGui::ColorEdit4("Coloring", (float*)&colorSelection, 0)) {
        Color newColor = colorFromFloat(colorSelection.x, colorSelection.y, colorSelection.z, 1.0f);
        resolveShader->setUniform("color", newColor);
        intensity = 0.01+2*colorSelection.w;
        numFragmentsMaxColor = std::max(maxComplexity, 4)/intensity;
        //reRender = true;
    }
}


void OIT_DepthComplexity::setNewState(const InternalState &newState)
{
    performanceMeasureMode = true;
}

bool OIT_DepthComplexity::needsReRender()
{
    // Update & print statistics if enough time has passed
    static float counterPrintFrags = 0.0f;
    counterPrintFrags += Timer->getElapsedSeconds();
    if (counterPrintFrags > 1.0f || firstFrame) {
        computeStatistics();
        counterPrintFrags = 0.0f;
        firstFrame = false;
        return true;
    }
    return false;
}

void OIT_DepthComplexity::computeStatistics()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    bufferSize = width * height;

    uint32_t *data = (uint32_t*)numFragmentsBuffer->mapBuffer(BUFFER_MAP_READ_ONLY);

    // Local reduction variables necessary for older OpenMP implementations
    uint64_t totalNumFragments = 0;
    uint64_t usedLocations = 0;
    uint64_t maxComplexity = 0;
    uint64_t minComplexity = 0;
    #pragma omp parallel for reduction(+:totalNumFragments,usedLocations) reduction(max:maxComplexity) reduction(min:minComplexity) schedule(static)
    for (int i = 0; i < bufferSize; i++) {
        totalNumFragments += data[i];
        if (data[i] > 0) {
            usedLocations++;
        }
        maxComplexity = std::max(maxComplexity, (uint64_t)data[i]);
        minComplexity = std::min(maxComplexity, (uint64_t)data[i]);
    }
    this->totalNumFragments = totalNumFragments;
    this->usedLocations = usedLocations;
    this->maxComplexity = maxComplexity;

    numFragmentsBuffer->unmapBuffer();

    if (!(performanceMeasureMode || recordingMode) || firstFrame) {
        firstFrame = false;
        numFragmentsMaxColor = std::max(maxComplexity, 4ul)/intensity;
    }

    if (performanceMeasureMode) {
        measurer->pushDepthComplexityFrame(minComplexity, maxComplexity,
                (float)totalNumFragments / usedLocations, (float)totalNumFragments / bufferSize, totalNumFragments);
    }

    if (totalNumFragments == 0) usedLocations = 1; // Avoid dividing by zero in code below
    std::cout << "Depth complexity: avg used: " << ((float)totalNumFragments / usedLocations)
              << ", avg all: " << ((float)totalNumFragments / bufferSize) << ", max: " << maxComplexity
              << ", #fragments: " << totalNumFragments << std::endl;
}
