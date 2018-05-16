/*
 * OIT_LinkedList.cpp
 *
 *  Created on: 14.05.2018
 *      Author: christoph
 */

#include "OIT_LinkedList.hpp"

#include <Math/Geometry/MatrixUtil.hpp>
#include "OIT_LinkedList.hpp"

using namespace sgl;

// Number of transparent pixels we can store per node
/*const int nodesPerPixel = 16;

void OIT_LinkedList::create()
{
	gatherShader = ShaderManager->getShaderProgram({"PixelSyncGather.Vertex", "PixelSyncGather.Fragment"});
	gatherShader->setUniform("nodesPerPixel", nodesPerPixel);

	blitShader = ShaderManager->getShaderProgram({"PixelSyncRender.Vertex", "PixelSyncRender.Fragment"});
	blitShader->setUniform("nodesPerPixel", nodesPerPixel);

	// Create blitting data (fullscreen rectangle in normalized device coordinates)
	blitRenderData = ShaderManager->createShaderAttributes(blitShader);

	std::vector<glm::vec3> fullscreenQuad{
		glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
		glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
	GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(), (void*)&fullscreenQuad.front());
	blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

	resolutionChanged();
}

void OIT_LinkedList::resolutionChanged()
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();

	size_t bufferSize = nodesPerPixel * width * height;
	size_t bufferSizeBytes = sizeof(FragmentNode) * bufferSize;
	fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

	gatherShader->setUniform("viewportW", width);
	gatherShader->setUniform("viewportH", height);
	gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);

	blitShader->setUniform("viewportW", width);
	blitShader->setUniform("viewportH", height);
	blitShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
}

void OIT_LinkedList::gatherBegin()
{
	;
}

void OIT_LinkedList::gatherEnd()
{
	;
}

void OIT_LinkedList::renderToScreen()
{
	glm::mat4 newProjMat(matrixOrthogonalProjection(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f));
	//setProjectionMatrix(newProjMat);

	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());

	Renderer->render(blitRenderData);
}*/

