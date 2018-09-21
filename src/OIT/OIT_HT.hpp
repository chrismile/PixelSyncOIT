//
// Created by christoph on 02.09.18.
//

#ifndef PIXELSYNCOIT_OIT_HT_HPP
#define PIXELSYNCOIT_OIT_HT_HPP

#include "OIT_Renderer.hpp"

#define HT_NUM_FRAGMENTS 8

// A fragment node stores rendering information about a list of fragments
struct HTFragmentNode_compressed
{
    // Linear depth, i.e. distance to viewer
    float depth[HT_NUM_FRAGMENTS];
    // RGB color (3 bytes), translucency (1 byte)
    uint premulColor[HT_NUM_FRAGMENTS];
};
struct HTFragmentTail_compressed
{
    // Accumulated alpha (16 bit) and fragment count (16 bit)
    uint accumAlphaAndCount;
    // RGB Color (30 bit, i.e. 10 bits per component)
    uint accumColor;
};

/**
 * An order independent transparency renderer using pixel sync.
 *
 * (To be precise, it doesn't use the Intel-specific Pixel Sync extension
 * INTEL_fragment_shader_ordering, but the vendor-independent ARB_fragment_shader_interlock).
 */
class OIT_HT : public OIT_Renderer {
public:
    /**
     *  The gather shader is used to render our transparent objects.
     *  Its purpose is to store the fragments in an offscreen-buffer.
     */
    virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    OIT_HT();
    virtual void create();
    virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO);

    virtual void gatherBegin();
    // In between "gatherBegin" and "gatherEnd", we can render our objects using the gather shader
    virtual void gatherEnd();

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

private:
    void clear();
    void setUniformData();

    sgl::GeometryBufferPtr fragmentNodes;
    sgl::GeometryBufferPtr fragmentTails;

    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
    sgl::ShaderAttributesPtr clearRenderData;

    bool clearBitSet;
};

#endif //PIXELSYNCOIT_OIT_HT_HPP
