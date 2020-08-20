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

#ifndef PIXELSYNCOIT_OIT_WBOIT_HPP
#define PIXELSYNCOIT_OIT_WBOIT_HPP

#include "OIT_Renderer.hpp"

/**
 * Based on: Morgan McGuire and Louis Bavoil, Weighted Blended Order-Independent Transparency, Journal of Computer
 * Graphics Techniques (JCGT), vol. 2, no. 2, 122-141, 2013.
 *
 * For more details regarding the implementation see:
 * http://casual-effects.blogspot.com/2015/03/implemented-weighted-blended-order.html
 */
class OIT_WBOIT : public OIT_Renderer {
public:
    /**
     *  The gather shader is used to render our transparent objects.
     *  Its purpose is to store the fragments in an offscreen-buffer.
     */
    virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    OIT_WBOIT();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
                                   sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    virtual void renderScene();
    virtual void gatherEnd();
    virtual void renderToScreen();
    void reloadShaders();

    virtual void renderGUI();

private:
    void clear();
    void setUniformData();

    // Render data of depth peeling
    sgl::FramebufferObjectPtr gatherPassFBO;
    sgl::TexturePtr accumulationRenderTexture;
    sgl::TexturePtr revealageRenderTexture;

    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
    sgl::ShaderProgramPtr resolveShader;

    // Render target of the final scene
    sgl::FramebufferObjectPtr sceneFramebuffer;
};

#endif //PIXELSYNCOIT_OIT_WBOIT_HPP
