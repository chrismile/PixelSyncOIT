/*
 * OIT_LinkedList.hpp
 *
 *  Created on: 14.05.2018
 *      Author: christoph
 */

#ifndef OIT_OIT_LINKEDLIST_HPP_
#define OIT_OIT_LINKEDLIST_HPP_

#include <glm/glm.hpp>

#include "OIT_Renderer.hpp"

// A fragment node stores rendering information about one specific fragment
struct LinkedListFragmentNode
{
	// RGBA color of the node
	uint32_t color;
	// Depth value of the fragment (in view space)
	float depth;
	// The index of the next node in "nodes" array
	uint32_t next;
};

/**
 * An order independent transparency renderer using a per-pixel linked list.
 * The algorithm uses two read-write buffers (SSBOs in OpenGL, UAVs in DirectX).
 * - The fragment-and-link buffer storing all rendered fragments in a linked list.
 * - The start-offset buffer storing the index of the first fragment in the buffer
 *   mentioned above for each pixel.
 */

const int linkedListEntriesPerPixel = 8;

class OIT_LinkedList : public OIT_Renderer {
public:
	/**
	 *  The gather shader is used to render our transparent objects.
	 *  Its purpose is to store the fragments in an offscreen-buffer.
	 */
	virtual sgl::ShaderProgramPtr getGatherShader() { return gatherShader; }

	OIT_LinkedList();
	virtual void create();
	virtual void resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::RenderbufferObjectPtr &sceneDepthRBO);

	virtual void gatherBegin();
	// In between "gatherBegin" and "gatherEnd", we can render our objects using the gather shader
	virtual void gatherEnd();

	// Blit accumulated transparent objects to screen
	virtual void renderToScreen();

	// OIT Renderers can render their own ImGui elements
	virtual void renderGUI();

	// For changing performance measurement modes
	void setNewState(const InternalState &newState);

private:
	void clear();
	void setUniformData();
	void setModeDefine();

	bool useNewShader = false;

	sgl::GeometryBufferPtr fragmentBuffer;
	sgl::GeometryBufferPtr startOffsetBuffer;
	sgl::GeometryBufferPtr atomicCounterBuffer;

	// Blit data (ignores model-view-projection matrix and uses normalized device coordinates)
	sgl::ShaderAttributesPtr blitRenderData;
	sgl::ShaderAttributesPtr clearRenderData;
};

#endif /* OIT_OIT_LINKEDLIST_HPP_ */
