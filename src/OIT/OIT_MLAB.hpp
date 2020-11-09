//
// Created by christoph on 29.08.18.
//

#ifndef PIXELSYNCOIT_OIT_MLAB_HPP
#define PIXELSYNCOIT_OIT_MLAB_HPP

#include "OIT_Renderer.hpp"

/**
 * An order independent transparency renderer using pixel sync (i.e., ARB_fragment_shader_interlock).
 *
 * M. Salvi and K. Vaidyanathan, “Multi-layer alpha blending,” in Proceedings of the 18th Meeting of the ACM SIGGRAPH
 * Symposium on Interactive 3D Graphics and Games, ser. I3D ’14. New York, NY, USA: ACM, 2014, pp. 151–158
 */
class OIT_MLAB : public OIT_Renderer {
public:
    /**
     *  The gather shader is used to render our transparent objects.
     *  Its purpose is to store the fragments in an offscreen-buffer.
     */
    virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    OIT_MLAB();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
            sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    // In between "gatherBegin" and "gatherEnd", we can render our objects using the gather shader
    virtual void gatherEnd();

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

    void renderGUI();
    void updateLayerMode();
    void reloadShaders();

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

private:
    void clear();
    void setUniformData();

    sgl::GeometryBufferPtr fragmentNodes;
    //sgl::GeometryBufferPtr numFragmentsBuffer;

    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
    sgl::ShaderAttributesPtr clearRenderData;

    sgl::FramebufferObjectPtr sceneFramebuffer;
    sgl::TexturePtr sceneTexture;
    sgl::RenderbufferObjectPtr sceneDepthRBO;

    bool clearBitSet;
};

#endif //PIXELSYNCOIT_OIT_MLAB_HPP
