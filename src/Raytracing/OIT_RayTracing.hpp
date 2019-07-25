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

#ifndef PIXELSYNCOIT_OIT_RAYTRACING_HPP
#define PIXELSYNCOIT_OIT_RAYTRACING_HPP

#include "../OIT/OIT_Renderer.hpp"
#include "../Utils/ImportanceCriteria.hpp"
#include "RTRenderBackend.hpp"

class OIT_RayTracing : public OIT_Renderer {
public:
    OIT_RayTracing(sgl::CameraPtr &camera, const sgl::Color &clearColor);
    virtual sgl::ShaderProgramPtr getGatherShader() { return sgl::ShaderProgramPtr(); }

    void create() {}
    void loadModel(
            int modelIndex, TrajectoryType trajectoryType, bool useTriangleMesh
            /*, std::vector<float> &attributes, float &maxAttribute*/);
    void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                           sgl::RenderbufferObjectPtr &sceneDepthRBO);
    void setLineRadius(float lineRadius);
    float getLineRadius();
    void setClearColor(const sgl::Color &clearColor);
    void setLightDirection(const glm::vec3 &lightDirection);
    void onTransferFunctionMapRebuilt();

    virtual void gatherBegin() {}
    virtual void renderScene() {}
    virtual void gatherEnd() {}
    virtual void setGatherShaderList(const std::list<std::string> &shaderIDs) {}

    // Contains all logic in raytracing renderer
    virtual void renderToScreen();
    void blitTexture();

    // Render options in GUI menu controlling parameters of ray caster
    virtual void renderGUI();

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

private:
    void fromFile(
            const std::string &filename, TrajectoryType trajectoryType, bool useTriangleMesh
            /*, std::vector<float> &attributes, float &maxAttribute*/);

    RTRenderBackend renderBackend;
    sgl::TexturePtr renderImage;

    // Data from MainApp
    sgl::CameraPtr camera;
    float lineRadius;
    sgl::Color clearColor;
    glm::vec3 lightDirection;

    bool useEmbree = true;
    bool changeTFN = false;
};


#endif //PIXELSYNCOIT_OIT_RAYTRACING_HPP
