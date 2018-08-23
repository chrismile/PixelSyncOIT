/*
 * OIT_DepthComplexity.hpp
 *
 *  Created on: 23.08.2018
 *      Author: Christoph Neuhauser
 */

#ifndef OIT_OIT_DEPTHCOMPLEXITY_HPP_
#define OIT_OIT_DEPTHCOMPLEXITY_HPP_

#include "OIT_Renderer.hpp"

/**
 * An order independent transparency renderer using pixel sync.
 *
 * (To be precise, it doesn't use the Intel-specific Pixel Sync extension
 * INTEL_fragment_shader_ordering, but the vendor-independent ARB_fragment_shader_interlock).
 */
class OIT_DepthComplexity : public OIT_Renderer {
public:
    /**
     *  The gather shader is used to render our transparent objects.
     *  Its purpose is to store the fragments in an offscreen-buffer.
     */
    virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    OIT_DepthComplexity();
    virtual void create();
    virtual void resolutionChanged();

    virtual void gatherBegin();
    // In between "gatherBegin" and "gatherEnd", we can render our objects using the gather shader
    virtual void gatherEnd();

    // Blit accumulated transparent objects to screen
    virtual void renderToScreen();

private:
    void clear();

    void computeStatistics();

    sgl::ShaderProgramPtr gatherShader;
    sgl::ShaderProgramPtr blitShader;
    sgl::ShaderProgramPtr clearShader;
    sgl::GeometryBufferPtr numFragmentsBuffer;

    // Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr blitRenderData;
    sgl::ShaderAttributesPtr clearRenderData;
};

#endif /* OIT_OIT_DEPTHCOMPLEXITY_HPP_ */
