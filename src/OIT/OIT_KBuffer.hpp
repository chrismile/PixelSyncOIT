/*
 * OIT_KBuffer.hpp
 *
 *  Created on: 14.05.2018
 *      Author: Christoph Neuhauser
 */

#ifndef OIT_OIT_PIXELSYNC_HPP_
#define OIT_OIT_PIXELSYNC_HPP_

#include "OIT_Renderer.hpp"

// A fragment node stores rendering information about one specific fragment
struct FragmentNode
{
	// RGBA color of the node
	uint32_t color;
	// Depth value of the fragment (in view space)
	float depth;
};

/**
 * An order independent transparency renderer using pixel sync.
 *
 * (To be precise, it doesn't use the Intel-specific Pixel Sync extension
 * INTEL_fragment_shader_ordering, but the vendor-independent ARB_fragment_shader_interlock).
 */
class OIT_KBuffer : public OIT_Renderer {
public:
	/**
	 *  The gather shader is used to render our transparent objects.
	 *  Its purpose is to store the fragments in an offscreen-buffer.
	 */
	virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

    OIT_KBuffer();
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
	sgl::GeometryBufferPtr numFragmentsBuffer;

	// Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
	sgl::ShaderAttributesPtr blitRenderData;
	sgl::ShaderAttributesPtr clearRenderData;

	sgl::FramebufferObjectPtr sceneFramebuffer;
	sgl::TexturePtr sceneTexture;
	sgl::RenderbufferObjectPtr sceneDepthRBO;
};

#endif /* OIT_OIT_PIXELSYNC_HPP_ */
