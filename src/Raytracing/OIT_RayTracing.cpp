/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Christoph Neuhauser
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

#include <chrono>
#include <GL/glew.h>

#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <Graphics/Scene/Camera.hpp>
#include <Utils/MeshSerializer.hpp>
#include <Utils/TrajectoryLoader.hpp>

#include "../Utils/TrajectoryFile.hpp"
#include "OIT_RayTracing.hpp"
#include "../OIT/BufferSizeWatch.hpp"

#include <Utils/File/FileUtils.hpp>
#include <Utils/File/Logfile.hpp>

#include "ospray/ospray.h"

static bool useEmbreeCurves = true;

OIT_RayTracing::OIT_RayTracing(sgl::CameraPtr &camera, const sgl::Color &clearColor)
        : camera(camera), clearColor(clearColor), renderBackend(useEmbreeCurves)
{
    // onTransferFunctionMapRebuilt();
    // ! Initialize OSPRay
    static bool ospray = false;
    int argc = sgl::FileUtils::get()->get_argc();
    char **argv = sgl::FileUtils::get()->get_argv();
    const char **av = const_cast<const char**>(argv);
    if(!ospray){
        OSPError init_error = ospInit(&argc, av);
        if (init_error != OSP_NO_ERROR){
            std::cout << "== ospray initialization fail" << std::endl;
            exit (EXIT_FAILURE);
        }else{
            std::cout << "== ospray initialized successfully" << std::endl;
        }
        ospLoadModule("tubes");
        ospray = true;
    }
}

void OIT_RayTracing::setLineRadius(float lineRadius)
{
    this->lineRadius = lineRadius;
    renderBackend.setLineRadius(lineRadius);
}

float OIT_RayTracing::getLineRadius()
{
    return this->lineRadius;
}

void OIT_RayTracing::setClearColor(const sgl::Color &clearColor)
{
    this->clearColor = clearColor;
}

void OIT_RayTracing::setLightDirection(const glm::vec3 &lightDirection)
{
    this->lightDirection = lightDirection;
}

void OIT_RayTracing::renderGUI()
{
    ImGui::Separator();

    if (ImGui::Checkbox("Embree curves", &useEmbreeCurves)) {
        renderBackend.setUseEmbreeCurves(useEmbreeCurves);
        loadModel(modelIndex, trajectoryType, useTriangleMesh);
        reRender = true;
    }
}

void OIT_RayTracing::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                                       sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    sgl::TextureSettings settings;
    renderImage = sgl::TextureManager->createEmptyTexture(width, height, settings);

    renderBackend.setViewportSize(width, height);
}

void OIT_RayTracing::loadModel(
        int modelIndex, TrajectoryType trajectoryType, bool useTriangleMesh
        /*, std::vector<float> &attributes, float &maxAttribute*/)
{
    this->modelIndex = modelIndex;
    this->trajectoryType = trajectoryType;
    this->useTriangleMesh = useTriangleMesh;
    fromFile(MODEL_FILENAMES[modelIndex], trajectoryType, useTriangleMesh);
}

void OIT_RayTracing::fromFile(
        const std::string &filename, TrajectoryType trajectoryType, bool useTriangleMesh
        /*, std::vector<float> &attributes, float &maxAttribute*/)
{
    auto startLoadFile = std::chrono::system_clock::now();

    if (useTriangleMesh) {
        std::cout << "---- file name is " << filename << std::endl;
        std::string modelFilenameBinmesh = sgl::FileUtils::get()->removeExtension(filename) + ".binmesh";
        BinaryMesh binmesh;
        if (!sgl::FileUtils::get()->exists(modelFilenameBinmesh)) {
            //convertTrajectoryDataToBinaryTriangleMesh(trajectoryType, filename, modelFilenameBinmesh, lineRadius);
            convertTrajectoryDataToBinaryTriangleMeshGPU(trajectoryType, filename, modelFilenameBinmesh, lineRadius);
        }
        readMesh3D(modelFilenameBinmesh, binmesh);
        BinarySubMesh &submesh = binmesh.submeshes.at(0);
        std::vector<uint32_t> indices = submesh.indices;
        submesh.indices.clear();
        submesh.indices.shrink_to_fit();
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> vertexNormals;
        std::vector<float> vertexAttributes;

        for (BinaryMeshAttribute &attr : submesh.attributes) {
            if (attr.name == "vertexPosition") {
                float *data = (float*)&attr.data.front();
                size_t n = attr.data.size()/(sizeof(glm::vec3));
                vertices.reserve(n);
                for (size_t i = 0; i < n; i++) {
                    vertices.push_back(glm::vec3(data[i*3+0], data[i*3+1], data[i*3+2]));
                }
            } else if (attr.name == "vertexNormal") {
                float *data = (float*)&attr.data.front();
                size_t n = attr.data.size()/(sizeof(glm::vec3));
                vertexNormals.reserve(n);
                for (size_t i = 0; i < n; i++) {
                    vertexNormals.push_back(glm::vec3(data[i*3+0], data[i*3+1], data[i*3+2]));
                }
            } else if (attr.name == "vertexAttribute0") {
                uint16_t *data = (uint16_t*)&attr.data.front();
                size_t n = attr.data.size()/(sizeof(uint16_t));
                vertexAttributes.reserve(n);
                for (size_t i = 0; i < n; i++) {
                    vertexAttributes.push_back(data[i] / 65535.0f);
                }
            }
            attr.data.clear();
            attr.data.shrink_to_fit();
        }
        binmesh = BinaryMesh();

        renderBackend.loadTriangleMesh(filename, indices, vertices, vertexNormals, vertexAttributes);
    } else {
        std::cout << "---- file name is " << filename << std::endl;
        Trajectories trajectories = loadTrajectoriesFromFile(filename, trajectoryType);
        renderBackend.loadTrajectories(filename, trajectories);
        onTransferFunctionMapRebuilt();
        renderBackend.setLineRadius(this->lineRadius);
    }

    onTransferFunctionMapRebuilt();

    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 invViewMatrix = glm::inverse(camera->getViewMatrix());
    glm::vec3 upDir = invViewMatrix[1];
    glm::vec3 lookDir = -invViewMatrix[2];
    glm::vec3 pos = invViewMatrix[3];

    auto startRTPreprocessing = std::chrono::system_clock::now();

    renderBackend.commitToOSPRay(pos, lookDir, upDir, camera->getFOVy());
    renderBackend.setCameraInitialize(lookDir);

    auto endRTPreprocesing = std::chrono::system_clock::now();
    auto rtPreprocessingElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endRTPreprocesing - startRTPreprocessing);
    sgl::Logfile::get()->writeInfo(std::string() + "Computational time to pre-process dataset for ray tracer: "
                                   + std::to_string(rtPreprocessingElapsedTime.count()));


    auto endLoadFile = std::chrono::system_clock::now();
    auto loadFileElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endLoadFile - startLoadFile);
    sgl::Logfile::get()->writeInfo(std::string() + "Total time to load file in ray tracer: "
                                   + std::to_string(loadFileElapsedTime.count()));

    setCurrentAlgorithmBufferSizeBytes(0);
}

void OIT_RayTracing::setNewState(const InternalState &newState)
{
    bool useEmbreeCurvesNew = useEmbreeCurves;
    newState.oitAlgorithmSettings.getValueOpt("useEmbreeCurves", useEmbreeCurvesNew);
    if (useEmbreeCurves != useEmbreeCurvesNew) {
        useEmbreeCurves = useEmbreeCurvesNew;
        renderBackend.setUseEmbreeCurves(useEmbreeCurves);
        loadModel(modelIndex, trajectoryType, useTriangleMesh);
    }
}

void OIT_RayTracing::renderToScreen()
{
    glm::vec3 linearRgbClearColorVec(clearColor.getFloatR(), clearColor.getFloatG(), clearColor.getFloatB());
    glm::vec3 sRgbClearColorVec = TransferFunctionWindow::linearRGBTosRGB(linearRgbClearColorVec);
    sgl::Color sRgbClearColor = sgl::colorFromFloat(sRgbClearColorVec.x, sRgbClearColorVec.y, sRgbClearColorVec.z);

    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 invViewMatrix = glm::inverse(camera->getViewMatrix());
    glm::vec3 upDir = invViewMatrix[1];
    glm::vec3 lookDir = -invViewMatrix[2];
    glm::vec3 pos = invViewMatrix[3];
    uint32_t *imageData = renderBackend.renderToImage(pos, lookDir, upDir, camera->getFOVy(), lineRadius, changeTFN);
    changeTFN = false;
    renderImage->uploadPixelData(width, height, imageData);

    blitTexture();
    reRender = true;
}

void OIT_RayTracing::blitTexture()
{
    glm::vec3 linearRgbClearColorVec(clearColor.getFloatR(), clearColor.getFloatG(), clearColor.getFloatB());
    glm::vec3 sRgbClearColorVec = TransferFunctionWindow::linearRGBTosRGB(linearRgbClearColorVec);
    sgl::Color sRgbClearColor = sgl::colorFromFloat(sRgbClearColorVec.x, sRgbClearColorVec.y, sRgbClearColorVec.z);

    // Blit the sRGB result image directly to the screen.
    sgl::Renderer->unbindFBO();
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Pre-multiplied alpha

    sgl::Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT, sRgbClearColor);
    sgl::Renderer->setProjectionMatrix(sgl::matrixIdentity());
    sgl::Renderer->setViewMatrix(sgl::matrixIdentity());
    sgl::Renderer->setModelMatrix(sgl::matrixIdentity());
    sgl::Renderer->blitTexture(renderImage, sgl::AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)));

    // Revert to normal alpha blending
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glDepthMask(GL_TRUE);
}

void OIT_RayTracing::onTransferFunctionMapRebuilt()
{
    std::vector<sgl::Color> tfLookupTable = g_TransferFunctionWindowHandle->getTransferFunctionMap_sRGB();
    renderBackend.setTransferFunction(tfLookupTable);
    if(changeTFN) changeTFN = false;
    else changeTFN = true;
}
