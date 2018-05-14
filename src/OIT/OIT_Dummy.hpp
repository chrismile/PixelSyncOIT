/*
 * OIT_Dummy.hpp
 *
 *  Created on: 14.05.2018
 *      Author: Christoph Neuhauser
 */

#ifndef OIT_OIT_DUMMY_HPP_
#define OIT_OIT_DUMMY_HPP_

#include "OIT_Renderer.hpp"

/**
 * The OIT dummy class just uses standard rendering.
 * If transparent pixels are rendered, the results are order-dependent.
 */
class OIT_Dummy : public OIT_Renderer
{
public:
	virtual sgl::ShaderProgramPtr getGatherShader() { return renderShader; }

	virtual void create() {
		renderShader = sgl::ShaderManager->getShaderProgram({"PseudoPhong.Vertex", "PseudoPhong.Fragment"});
	}
	virtual void resolutionChanged() {}

	virtual void gatherBegin() {}
	// In between "gatherBegin" and "gatherEnd", we can render our objects using the gather shader
	virtual void gatherEnd() {}

	/**
	 * Blit accumulated transparent objects to screen.
	 * Disclaimer: May change view/projection matrices!
	 */
	virtual void renderToScreen() {}

private:
	sgl::ShaderProgramPtr renderShader; // == gather shader (normal rendering)
};

#endif /* OIT_OIT_DUMMY_HPP_ */
