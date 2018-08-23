/*
 * OIT_LinkedList.cpp
 *
 *  Created on: 14.05.2018
 *      Author: christoph
 */

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "OIT_LinkedList.hpp"

using namespace sgl;

// Number of transparent pixels we can store per node
const int nodesPerPixel = 8;

OIT_LinkedList::OIT_LinkedList()
{
	create();
}

void OIT_LinkedList::create()
{
	gatherShader = ShaderManager->getShaderProgram({"LinkedListGather.Vertex", "LinkedListGather.Fragment"});
	blitShader = ShaderManager->getShaderProgram({"LinkedListRender.Vertex", "LinkedListRender.Fragment"});
	clearShader = ShaderManager->getShaderProgram({"LinkedListClear.Vertex", "LinkedListClear.Fragment"});

	// Create blitting data (fullscreen rectangle in normalized device coordinates)
	blitRenderData = ShaderManager->createShaderAttributes(blitShader);

	std::vector<glm::vec3> fullscreenQuad{
		glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
		glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
	GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
	blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

	clearRenderData = ShaderManager->createShaderAttributes(clearShader);
	geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
	clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
}

void OIT_LinkedList::resolutionChanged()
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();

	size_t fragmentBufferSize = nodesPerPixel * width * height;
	size_t fragmentBufferSizeBytes = sizeof(LinkedListFragmentNode) * fragmentBufferSize;
	void *data = (void*)malloc(fragmentBufferSizeBytes);
	memset(data, 0, fragmentBufferSizeBytes);

	fragmentBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	fragmentBuffer = Renderer->createGeometryBuffer(fragmentBufferSizeBytes, data, SHADER_STORAGE_BUFFER);
	free(data);

	size_t startOffsetBufferSizeBytes = sizeof(uint32_t) * width * height;
	startOffsetBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	startOffsetBuffer = Renderer->createGeometryBuffer(startOffsetBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

	atomicCounterBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	atomicCounterBuffer = Renderer->createGeometryBuffer(sizeof(uint32_t), NULL, SHADER_STORAGE_BUFFER);


	gatherShader->setUniform("viewportW", width);
	//gatherShader->setUniform("viewportH", height); // Not needed
	gatherShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
	gatherShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);
	gatherShader->setAtomicCounterBuffer(0, "fragCounter", atomicCounterBuffer);
	gatherShader->setUniform("linkedListSize", (int)fragmentBufferSize);

	blitShader->setUniform("viewportW", width);
	//blitShader->setUniform("viewportH", height); // Not needed
	blitShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
	blitShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);
	blitShader->setAtomicCounterBuffer(0, "fragCounter", atomicCounterBuffer);
	//blitShader->setUniform("linkedListSize", (int)fragmentBufferSize);

	clearShader->setUniform("viewportW", width);
	//clearShader->setUniform("viewportH", height); // Not needed
	clearShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
	clearShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);
	clearShader->setAtomicCounterBuffer(0, "fragCounter", atomicCounterBuffer);
	//clearShader->setUniform("linkedListSize", (int)fragmentBufferSize);
}

void OIT_LinkedList::gatherBegin()
{
	//glClearDepth(0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());
	//glEnable(GL_RASTERIZER_DISCARD);
	Renderer->render(clearRenderData);
	/*GLuint bufferID = static_cast<GeometryBufferGL*>(fragmentNodes.get())->getBuffer();
	GLubyte val = 0;
	glClearNamedBufferData(bufferID, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, (const void*)&val);*/
	GLuint bufferID = static_cast<GeometryBufferGL*>(atomicCounterBuffer.get())->getBuffer();
	GLubyte val = 0;
	glClearNamedBufferData(bufferID, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, (const void*)&val);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_FALSE);
}

void OIT_LinkedList::gatherEnd()
{
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_LinkedList::renderToScreen()
{
	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());

	//glDisable(GL_RASTERIZER_DISCARD);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_DEPTH_TEST);

	Renderer->render(blitRenderData);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glDepthMask(GL_TRUE);
}
