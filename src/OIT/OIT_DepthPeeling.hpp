//
// Created by christoph on 21.09.18.
//

#ifndef PIXELSYNCOIT_OIT_DEPTHPEELING_HPP
#define PIXELSYNCOIT_OIT_DEPTHPEELING_HPP

#include "OIT_Renderer.hpp"

/**
 * Depth peeling: C. Everitt, "Interactive order-independent transparency", NVIDIA Corporation, vol. 2, 10 2001.
 */
class OIT_DepthPeeling : public OIT_Renderer {
public:
    /**
     *  The gather shader is used to render our transparent objects.
     *  Its purpose is to store the fragments in an offscreen-buffer.
     */
    virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    OIT_DepthPeeling();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
            sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    virtual void renderScene();
    virtual void gatherEnd();

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

    virtual void renderGUI();

    virtual void setGatherShaderList(const std::vector<std::string> &shaderIDs)
    {
        gatherShaderIDs = shaderIDs;
        sgl::ShaderManager->invalidateShaderCache();
        sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"DepthPeelingGather.glsl\"");
        gatherShader = sgl::ShaderManager->getShaderProgram(gatherShaderIDs);
        sgl::ShaderManager->invalidateShaderCache();
        sgl::ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"DepthComplexityGather.glsl\"");
        depthComplexityGatherShader = sgl::ShaderManager->getShaderProgram(gatherShaderIDs);
    }

private:
    void setUniformData();
    void computeDepthComplexity();
    uint64_t maxDepthComplexity = 1;

    // Render data of depth peeling
    sgl::FramebufferObjectPtr depthPeelingFBOs[2];
    sgl::TexturePtr colorRenderTextures[2];
    sgl::TexturePtr depthRenderTextures[2];
    sgl::FramebufferObjectPtr accumulatorFBO;
    sgl::TexturePtr colorAccumulatorTexture;

    // For computing the depth complexity
    sgl::ShaderProgramPtr depthComplexityGatherShader;
    sgl::GeometryBufferPtr numFragmentsBuffer;

    // Render target of the final scene
    sgl::FramebufferObjectPtr sceneFramebuffer;
};


#endif //PIXELSYNCOIT_OIT_DEPTHPEELING_HPP
