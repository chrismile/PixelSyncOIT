/*
 * OIT_PixelSync.cpp
 *
 *  Created on: 14.05.2018
 *      Author: Christoph Neuhauser
 */

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/OpenGL/GeometryBuffer.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "OIT_PixelSync.hpp"

using namespace sgl;

// Number of transparent pixels we can store per node
const int nodesPerPixel = 8;

// Use stencil buffer to mask unused pixels
const bool useStencilBuffer = true;

OIT_PixelSync::OIT_PixelSync()
{
	create();
}

void OIT_PixelSync::create()
{
	if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
		Logfile::get()->writeError("Error in OIT_PixelSync::create: GL_ARB_fragment_shader_interlock unsupported.");
		exit(1);
	}

	gatherShader = ShaderManager->getShaderProgram({"PixelSyncGather.Vertex", "PixelSyncGather.Fragment"});
	gatherShader->setUniform("nodesPerPixel", nodesPerPixel);

	blitShader = ShaderManager->getShaderProgram({"PixelSyncRender.Vertex", "PixelSyncRender.Fragment"});
	blitShader->setUniform("nodesPerPixel", nodesPerPixel);

	clearShader = ShaderManager->getShaderProgram({"PixelSyncClear.Vertex", "PixelSyncClear.Fragment"});
	//clearShader->setUniform("nodesPerPixel", nodesPerPixel);

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

void OIT_PixelSync::resolutionChanged()
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();

	size_t bufferSize = nodesPerPixel * width * height;
	size_t bufferSizeBytes = sizeof(FragmentNode) * bufferSize;
	void *data = (void*)malloc(bufferSizeBytes);
	memset(data, 0, bufferSizeBytes);

	fragmentNodes = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, data, SHADER_STORAGE_BUFFER);
	free(data);

	size_t numFragmentsBufferSizeBytes = sizeof(int32_t) * width * height;
	numFragmentsBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	numFragmentsBuffer = Renderer->createGeometryBuffer(numFragmentsBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

	gatherShader->setUniform("viewportW", width);
	//gatherShader->setUniform("viewportH", height); // Not needed
	gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
	gatherShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

	blitShader->setUniform("viewportW", width);
	//blitShader->setUniform("viewportH", height); // Not needed
	blitShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
	blitShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

	clearShader->setUniform("viewportW", width);
	//clearShader->setUniform("viewportH", height); // Not needed
	clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
	clearShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);
}

void OIT_PixelSync::gatherBegin()
{
	//glClearDepth(0.0);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_LESS);
	//glDepthMask(GL_FALSE);

	if (useStencilBuffer) {
		glClear(GL_STENCIL_BUFFER_BIT);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF);
	}
}

void OIT_PixelSync::gatherEnd()
{
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_PixelSync::renderToScreen()
{
	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());

	//glDisable(GL_RASTERIZER_DISCARD);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_DEPTH_TEST);

	if (useStencilBuffer) {
		glStencilFunc(GL_EQUAL, 1, 0xFF);
		glStencilMask(0x00);
	}

	Renderer->render(blitRenderData);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glDepthMask(GL_TRUE);
}
