/*
 * OIT_KBuffer.cpp
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
#include <ImGui/ImGuiWrapper.hpp>

#include "TilingMode.hpp"
#include "OIT_KBuffer.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
static bool useStencilBuffer = true;

// Maximum number of layers
static int maxNumNodes = 8;


OIT_KBuffer::OIT_KBuffer()
{
    create();
}

void OIT_KBuffer::create()
{
	if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
		Logfile::get()->writeError("Error in OIT_KBuffer::create: GL_ARB_fragment_shader_interlock unsupported.");
		exit(1);
	}

	ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"KBufferGather.glsl\"");

	updateLayerMode();

	// Create blitting data (fullscreen rectangle in normalized device coordinates)
	blitRenderData = ShaderManager->createShaderAttributes(resolveShader);

	std::vector<glm::vec3> fullscreenQuad{
		glm::vec3(1,1,0), glm::vec3(-1,-1,0), glm::vec3(1,-1,0),
		glm::vec3(-1,-1,0), glm::vec3(1,1,0), glm::vec3(-1,1,0)};
	GeometryBufferPtr geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
			(void*)&fullscreenQuad.front());
	blitRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);

	clearRenderData = ShaderManager->createShaderAttributes(clearShader);
	geomBuffer = Renderer->createGeometryBuffer(sizeof(glm::vec3)*fullscreenQuad.size(),
			(void*)&fullscreenQuad.front());
	clearRenderData->addGeometryBuffer(geomBuffer, "vertexPosition", ATTRIB_FLOAT, 3);
}

void OIT_KBuffer::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
		sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
	this->sceneFramebuffer = sceneFramebuffer;
	this->sceneTexture = sceneTexture;
	this->sceneDepthRBO = sceneDepthRBO;

	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();

	size_t bufferSizeBytes = sizeof(FragmentNode) * maxNumNodes * width * height;
	void *data = (void*)malloc(bufferSizeBytes);
	memset(data, 0, bufferSizeBytes);

	fragmentNodes = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	fragmentNodes = Renderer->createGeometryBuffer(bufferSizeBytes, data, SHADER_STORAGE_BUFFER);
	free(data);

	size_t numFragmentsBufferSizeBytes = sizeof(int32_t) * width * height;
	numFragmentsBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	numFragmentsBuffer = Renderer->createGeometryBuffer(numFragmentsBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);
}


void OIT_KBuffer::renderGUI()
{
	ImGui::Separator();

	if (ImGui::SliderInt("Num Layers", &maxNumNodes, 1, 64)) {
		updateLayerMode();
		reRender = true;
	}

	if (selectTilingModeUI()) {
		reloadShaders();
		reRender = true;
	}
}

void OIT_KBuffer::updateLayerMode()
{
	ShaderManager->invalidateShaderCache();
	ShaderManager->addPreprocessorDefine("MAX_NUM_NODES", maxNumNodes);
	reloadShaders();
	resolutionChanged(sceneFramebuffer, sceneTexture, sceneDepthRBO);
}

void OIT_KBuffer::reloadShaders()
{
	gatherShader = ShaderManager->getShaderProgram({gatherShaderName + ".Vertex", gatherShaderName + ".Fragment"});
	resolveShader = ShaderManager->getShaderProgram({"KBufferResolve.Vertex", "KBufferResolve.Fragment"});
	clearShader = ShaderManager->getShaderProgram({"KBufferClear.Vertex", "KBufferClear.Fragment"});
	//needsNewShaders = true;

	if (blitRenderData) {
		blitRenderData = blitRenderData->copy(resolveShader);
	}
	if (clearRenderData) {
		clearRenderData = clearRenderData->copy(clearShader);
	}
}


void OIT_KBuffer::setNewState(const InternalState &newState)
{
	maxNumNodes = newState.oitAlgorithmSettings.getIntValue("numLayers");
	useStencilBuffer = newState.useStencilBuffer;
	updateLayerMode();
}



void OIT_KBuffer::setUniformData()
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();

	gatherShader->setUniform("viewportW", width);
	gatherShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
	gatherShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

	resolveShader->setUniform("viewportW", width);
	resolveShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
	resolveShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);

	clearShader->setUniform("viewportW", width);
	clearShader->setShaderStorageBuffer(0, "FragmentNodes", fragmentNodes);
	clearShader->setShaderStorageBuffer(1, "NumFragmentsBuffer", numFragmentsBuffer);
}

void OIT_KBuffer::gatherBegin()
{
	setUniformData();

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());
	Renderer->render(clearRenderData);
	/*GLuint bufferID = static_cast<GeometryBufferGL*>(fragmentNodes.get())->getBuffer();
	GLubyte val = 0;
	glClearNamedBufferData(bufferID, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, (const void*)&val);*/
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glEnable(GL_DEPTH_TEST);

	if (useStencilBuffer) {
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF);
		glClear(GL_STENCIL_BUFFER_BIT);
	}
}

void OIT_KBuffer::gatherEnd()
{
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void OIT_KBuffer::renderToScreen()
{
	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_DEPTH_TEST);

	if (useStencilBuffer) {
		glStencilFunc(GL_EQUAL, 1, 0xFF);
		glStencilMask(0x00);
	}

	Renderer->render(blitRenderData);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glDisable(GL_STENCIL_TEST);
	glDepthMask(GL_TRUE);
}
