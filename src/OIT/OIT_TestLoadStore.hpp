//
// Created by christoph on 22.09.18.
//

#ifndef PIXELSYNCOIT_OIT_TESTLOADSTORE_HPP
#define PIXELSYNCOIT_OIT_TESTLOADSTORE_HPP


#include "OIT_Renderer.hpp"

/**
 * The OIT dummy class just uses standard rendering.
 * If transparent pixels are rendered, the results are order-dependent.
 */
class OIT_TestLoadStore : public OIT_Renderer
{
public:
    virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    OIT_TestLoadStore();
    virtual ~OIT_TestLoadStore();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin() {}
    virtual void renderScene() {  }
    virtual void gatherEnd();

    /**
     * Blit accumulated transparent objects to screen.
     * Disclaimer: May change view/projection matrices!
     */
    virtual void renderToScreen() {}

private:
    sgl::ShaderProgramPtr writeShader;
    sgl::ShaderProgramPtr readShader;
    sgl::ShaderAttributesPtr writeRenderData;
    sgl::ShaderAttributesPtr readRenderData;

    sgl::TexturePtr moments;
};


#endif //PIXELSYNCOIT_OIT_TESTLOADSTORE_HPP
