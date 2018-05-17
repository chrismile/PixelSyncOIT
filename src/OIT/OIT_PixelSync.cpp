/*
 * OIT_PixelSync.cpp
 *
 *  Created on: 14.05.2018
 *      Author: Christoph Neuhauser
 */

#include <cstdlib>
#include <cstring>

#include <Utils/File/Logfile.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/OpenGL/SystemGL.hpp>
#include <Graphics/OpenGL/Shader.hpp>

#include "OIT_PixelSync.hpp"

using namespace sgl;

// Number of transparent pixels we can store per node
const int nodesPerPixel = 1;

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
	clearShader->setUniform("nodesPerPixel", nodesPerPixel);

	// Create blitting data (fullscreen rectangle in normalized device coordinates)
	blitRenderData = ShaderManager->createShaderAttributes(blitShader);

	std::vector<glm::vec3> fullscreenQuad{
		glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
		glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
	GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
	blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

	// TODO
	clearRenderData = ShaderManager->createShaderAttributes(clearShader);
	geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
	clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
	//clearRenderData = blitRenderData->copy(clearShader); // faulty


	resolutionChanged();

	std::cout << "sizeof(FragmentNode): " << sizeof(FragmentNode) << std::endl;

	GLuint progID = static_cast<ShaderProgramGL*>(clearShader.get())->getShaderProgramID();
	GLuint bufferIndex = glGetProgramResourceIndex(progID, GL_SHADER_STORAGE_BLOCK, "FragmentNodes");

	GLenum prop = GL_BUFFER_DATA_SIZE;
	GLsizei bufSize = 1;
	GLint param;
	glGetProgramResourceiv(progID, GL_SHADER_STORAGE_BLOCK, bufferIndex, 1, &prop, bufSize, NULL, &param);
	std::cout << "Data size: " << param << std::endl;
}

void OIT_PixelSync::resolutionChanged()
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();

	size_t bufferSize = nodesPerPixel * width * height;
	size_t bufferSizeBytes = sizeof(FragmentNode) * bufferSize;
	void *data = (void*)malloc(bufferSizeBytes);
	memset(data, 255, bufferSizeBytes);
	fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, data/*NULL*/, SHADER_STORAGE_BUFFER);

	gatherShader->setUniform("viewportW", width);
	//gatherShader->setUniform("viewportH", height); // Not needed
	gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

	blitShader->setUniform("viewportW", width);
	//blitShader->setUniform("viewportH", height); // Not needed
	blitShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

	clearShader->setUniform("viewportW", width);
	//clearShader->setUniform("viewportH", height); // Not needed
	clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
}

void OIT_PixelSync::gatherBegin()
{
	glm::mat4 newProjMat(matrixOrthogonalProjection(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f));
	Renderer->setProjectionMatrix(newProjMat);
	//Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());
	//glEnable(GL_RASTERIZER_DISCARD);
	Renderer->render(clearRenderData);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	//glMemoryBarrier(GL_ALL_BARRIER_BITS);

	glEnable(GL_DEPTH_TEST);
}

void OIT_PixelSync::gatherEnd()
{
	glDisable(GL_DEPTH_TEST);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	//glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void OIT_PixelSync::renderToScreen()
{
	glm::mat4 newProjMat(matrixOrthogonalProjection(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f));
	Renderer->setProjectionMatrix(newProjMat);
	//Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());

	//glDisable(GL_RASTERIZER_DISCARD);
	glClear(GL_COLOR_BUFFER_BIT);

	Renderer->render(blitRenderData);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
}
