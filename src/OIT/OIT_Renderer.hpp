/*
 * OIT_Renderer.hpp
 *
 *  Created on: 14.05.2018
 *      Author: Christoph Neuhauser
 */

#ifndef OIT_OIT_RENDERER_HPP_
#define OIT_OIT_RENDERER_HPP_

#include <cstddef>
#include <functional>
#include <glm/glm.hpp>

#include <Utils/AppSettings.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>

/**
 * An interface class for order independent transparency renderers.
 */
class OIT_Renderer
{
public:
	virtual ~OIT_Renderer() {}
	/**
	 *  The gather shader is used to render our transparent objects.
	 *  Its purpose is to store the fragments in an offscreen-buffer.
	 */
	virtual sgl::ShaderProgramPtr getGatherShader()=0;


	virtual void create()=0;
	virtual void resolutionChanged()=0;

	// In between "gatherBegin" and "gatherEnd", we can render our objects using the gather shader
	virtual void gatherBegin()=0;
	virtual void renderScene() { renderSceneFunction(); } // Uses renderSceneFunction. Override in e.g. MBOIT.
	virtual void gatherEnd()=0;

	// Blit accumulated transparent objects to screen
	virtual void renderToScreen()=0;

	/**
	 * Set the function which renders the scene. Necessary for algorithms like MBOIT which need two passes.
	 */
	inline void setRenderSceneFunction(std::function<void()> renderSceneFunction)
	{
		this->renderSceneFunction = renderSceneFunction;
	}

protected:
	std::function<void()> renderSceneFunction;
};

#endif /* OIT_OIT_RENDERER_HPP_ */
