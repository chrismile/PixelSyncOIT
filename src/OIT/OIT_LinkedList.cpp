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
#include <ImGui/ImGuiWrapper.hpp>

#include "OIT_LinkedList.hpp"

using namespace sgl;

// Use stencil buffer to mask unused pixels
static bool useStencilBuffer = true;

/// Expected (average) depth complexity, i.e. width*height* this value = number of fragments that can be stored
static int expectedDepthComplexity = 8;
/// Maximum number of fragments to sort in second pass
static int maxNumFragmentsSorting = 8;

// Choice of sorting algorithm
static int algorithmMode = 0;

OIT_LinkedList::OIT_LinkedList()
{
	create();
}

void OIT_LinkedList::create()
{
	setModeDefine();
	ShaderManager->addPreprocessorDefine("OIT_GATHER_HEADER", "\"LinkedListGather.glsl\"");
	ShaderManager->addPreprocessorDefine("MAX_NUM_FRAGS", toString(maxNumFragmentsSorting));

	std::list<std::string> shaderIDs = {gatherShaderName + ".Vertex", gatherShaderName + ".Fragment"};
	if (gatherShaderName.find("Vorticity") != std::string::npos) {
		shaderIDs.push_back(gatherShaderName + ".Geometry");
	}
	gatherShader = ShaderManager->getShaderProgram(shaderIDs);
	resolveShader = ShaderManager->getShaderProgram({"LinkedListResolve.Vertex", "LinkedListResolve.Fragment"});
	clearShader = ShaderManager->getShaderProgram({"LinkedListClear.Vertex", "LinkedListClear.Fragment"});

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

void OIT_LinkedList::resolutionChanged(sgl::FramebufferObjectPtr &sceneFramebuffer, sgl::TexturePtr &sceneTexture,
		sgl::RenderbufferObjectPtr &sceneDepthRBO)
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();

	size_t fragmentBufferSize = expectedDepthComplexity * width * height;
	size_t fragmentBufferSizeBytes = sizeof(LinkedListFragmentNode) * fragmentBufferSize;

	fragmentBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	fragmentBuffer = Renderer->createGeometryBuffer(fragmentBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

	size_t startOffsetBufferSizeBytes = sizeof(uint32_t) * width * height;
	startOffsetBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	startOffsetBuffer = Renderer->createGeometryBuffer(startOffsetBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

	atomicCounterBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
	if (testNoAtomicOperations) {
		atomicCounterBuffer = Renderer->createGeometryBuffer(sizeof(uint32_t), NULL, SHADER_STORAGE_BUFFER);
	} else {
		atomicCounterBuffer = Renderer->createGeometryBuffer(sizeof(uint32_t), NULL, ATOMIC_COUNTER_BUFFER);
	}
}

void OIT_LinkedList::setUniformData()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    size_t fragmentBufferSize = expectedDepthComplexity * width * height;
    size_t fragmentBufferSizeBytes = sizeof(LinkedListFragmentNode) * fragmentBufferSize;

    gatherShader->setUniform("viewportW", width);
    gatherShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
    gatherShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);
	if (testNoAtomicOperations) {
		gatherShader->setShaderStorageBuffer(2, "FragCounterBuffer", atomicCounterBuffer);
	}
	else {
		gatherShader->setAtomicCounterBuffer(0, atomicCounterBuffer);
	}
    gatherShader->setUniform("linkedListSize", (int)fragmentBufferSize);

    resolveShader->setUniform("viewportW", width);
    resolveShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
    resolveShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);

    clearShader->setUniform("viewportW", width);
    clearShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);
}


void OIT_LinkedList::renderGUI()
{
	ImGui::Separator();

	if (ImGui::SliderInt("Avg. Depth", &expectedDepthComplexity, 1, 128)) {
		Window *window = AppSettings::get()->getMainWindow();
		int width = window->getWidth();
		int height = window->getHeight();
		size_t fragmentBufferSize = expectedDepthComplexity * width * height;
		size_t fragmentBufferSizeBytes = sizeof(LinkedListFragmentNode) * fragmentBufferSize;
		fragmentBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
		fragmentBuffer = Renderer->createGeometryBuffer(fragmentBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

		gatherShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
		resolveShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
		gatherShader->setUniform("linkedListSize", (int)fragmentBufferSize);

		reRender = true;
	}

	// If something changes about fragment collection & sorting
	bool needNewResolveShader = false;

	if (ImGui::SliderInt("Num Sort", &maxNumFragmentsSorting, 1, 2000)) {
		ShaderManager->invalidateShaderCache();
		ShaderManager->addPreprocessorDefine("MAX_NUM_FRAGS", toString(maxNumFragmentsSorting));
		needNewResolveShader = true;
		reRender = true;
	}

	if (ImGui::Combo("Sorting Mode", &algorithmMode, sortingModeStrings, IM_ARRAYSIZE(sortingModeStrings))) {
		ShaderManager->invalidateShaderCache();
		setModeDefine();
		needNewResolveShader = true;
		reRender = true;
	}

	// --- END OF GUI CODE ---


	if (needNewResolveShader) {
		resolveShader = ShaderManager->getShaderProgram({"LinkedListResolve.Vertex", "LinkedListResolve.Fragment"});
		resolveShader->setUniform("viewportW", AppSettings::get()->getMainWindow()->getWidth());
		//resolveShader->setUniform("viewportH", height); // Not needed
		resolveShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
		resolveShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);

		blitRenderData = blitRenderData->copy(resolveShader);
	}
}

void OIT_LinkedList::setModeDefine()
{
	if (algorithmMode == 0) {
		ShaderManager->addPreprocessorDefine("sortingAlgorithm", "frontToBackPQ");
	} else if (algorithmMode == 1) {
		ShaderManager->addPreprocessorDefine("sortingAlgorithm", "bubbleSort");
	} else if (algorithmMode == 2) {
		ShaderManager->addPreprocessorDefine("sortingAlgorithm", "insertionSort");
	} else if (algorithmMode == 3) {
		ShaderManager->addPreprocessorDefine("sortingAlgorithm", "shellSort");
	} else if (algorithmMode == 4) {
		ShaderManager->addPreprocessorDefine("sortingAlgorithm", "heapSort");
	}
}


void OIT_LinkedList::setNewState(const InternalState &newState)
{
	useStencilBuffer = newState.useStencilBuffer;
	testNoAtomicOperations = newState.testNoAtomicOperations;

	if (expectedDepthComplexity != newState.oitAlgorithmSettings.getIntValue("expectedDepthComplexity")) {
		expectedDepthComplexity = newState.oitAlgorithmSettings.getIntValue("expectedDepthComplexity");

		Window *window = AppSettings::get()->getMainWindow();
		int width = window->getWidth();
		int height = window->getHeight();
		size_t fragmentBufferSize = expectedDepthComplexity * width * height;
		size_t fragmentBufferSizeBytes = sizeof(LinkedListFragmentNode) * fragmentBufferSize;
		fragmentBuffer = sgl::GeometryBufferPtr(); // Delete old data first (-> refcount 0)
		fragmentBuffer = Renderer->createGeometryBuffer(fragmentBufferSizeBytes, NULL, SHADER_STORAGE_BUFFER);

		gatherShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
		resolveShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
		gatherShader->setUniform("linkedListSize", (int)fragmentBufferSize);
	}

	bool needNewResolveShader = false;

	if (maxNumFragmentsSorting != newState.oitAlgorithmSettings.getIntValue("maxNumFragmentsSorting")) {
		maxNumFragmentsSorting = newState.oitAlgorithmSettings.getIntValue("maxNumFragmentsSorting");
		ShaderManager->invalidateShaderCache();
		ShaderManager->addPreprocessorDefine("MAX_NUM_FRAGS", toString(maxNumFragmentsSorting));
		needNewResolveShader = true;
		reRender = true;
	}


	// Set sorting mode
	std::string newSortingModeString = newState.oitAlgorithmSettings.getValue("sortingMode");
	for (int i = 0; i < IM_ARRAYSIZE(sortingModeStrings); i++) {
		if (sortingModeStrings[i] == newSortingModeString) {
			algorithmMode = i;
			ShaderManager->invalidateShaderCache();
			setModeDefine();
			needNewResolveShader = true;
		}
	}

	if (needNewResolveShader) {
		resolveShader = ShaderManager->getShaderProgram({"LinkedListResolve.Vertex", "LinkedListResolve.Fragment"});
		resolveShader->setUniform("viewportW", AppSettings::get()->getMainWindow()->getWidth());
		//resolveShader->setUniform("viewportH", height); // Not needed
		resolveShader->setShaderStorageBuffer(0, "FragmentBuffer", fragmentBuffer);
		resolveShader->setShaderStorageBuffer(1, "StartOffsetBuffer", startOffsetBuffer);

		blitRenderData = blitRenderData->copy(resolveShader);
	}
}



void OIT_LinkedList::gatherBegin()
{
    setUniformData();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());
	Renderer->render(clearRenderData);

	// Set atomic counter to zero
	GLuint bufferID = static_cast<GeometryBufferGL*>(atomicCounterBuffer.get())->getBuffer();
	GLubyte val = 0;
	glClearNamedBufferData(bufferID, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, (const void*)&val);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_FALSE);

    if (useStencilBuffer) {
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF);
		glClear(GL_STENCIL_BUFFER_BIT);
    }
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

