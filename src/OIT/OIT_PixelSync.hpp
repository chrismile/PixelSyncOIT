/*
 * OIT_PixelSync.hpp
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
	glm::vec4 color;
	// Depth value of the fragment (in view space)
	float depth;
	// Whether the node is empty or used
	uint used;

	// Padding to 2*vec4
	//uint padding1;
	//uint padding2;
};

/**
 * An order independent transparency renderer using pixel sync.
 *
 * (To be precise, it doesn't use the Intel-specific Pixel Sync extension
 * INTEL_fragment_shader_ordering, but the vendor-independent ARB_fragment_shader_interlock).
 */
class OIT_PixelSync : public OIT_Renderer {
public:
	/**
	 *  The gather shader is used to render our transparent objects.
	 *  Its purpose is to store the fragments in an offscreen-buffer.
	 */
	virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

	virtual void create();
	virtual void resolutionChanged();

	virtual void gatherBegin();
	// In between "gatherBegin" and "gatherEnd", we can render our objects using the gather shader
	virtual void gatherEnd();

	// Blit accumulated transparent objects to screen
	virtual void renderToScreen();

private:
	void clear();

	sgl::ShaderProgramPtr gatherShader;
	sgl::ShaderProgramPtr blitShader;
	sgl::GeometryBufferPtr fragmentNodes;

	// Blits data (ignores model-view-projection matrix and uses normalized device coordinates)
	sgl::ShaderAttributesPtr blitRenderData;
};

#endif /* OIT_OIT_PIXELSYNC_HPP_ */
