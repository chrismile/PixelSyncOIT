/*
 * MainApp.cpp
 *
 *  Created on: 22.04.2017
 *      Author: Christoph Neuhauser
 */

#define GLM_ENABLE_EXPERIMENTAL
#include <climits>
#include <glm/gtx/color_space.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <GL/glew.h>

#include <Input/Keyboard.hpp>
#include <Math/Math.hpp>
#include <Math/Geometry/MatrixUtil.hpp>
#include <Graphics/Window.hpp>
#include <Utils/AppSettings.hpp>
#include <Utils/Events/EventManager.hpp>
#include <Utils/Random/Xorshift.hpp>
#include <Utils/Timer.hpp>
#include <Utils/File/FileUtils.hpp>
#include <Input/Mouse.hpp>
#include <Input/Keyboard.hpp>
#include <Utils/File/Logfile.hpp>
#include <Utils/File/FileUtils.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/Shader/ShaderManager.hpp>

#include "Utils/MeshSerializer.hpp"
#include "Utils/OBJLoader.hpp"
#include "OIT/OIT_Dummy.hpp"
#include "OIT/OIT_PixelSync.hpp"
#include "MainApp.hpp"

PixelSyncApp::PixelSyncApp() : camera(new Camera()), recording(false), videoWriter(NULL)
{
	plainShader = ShaderManager->getShaderProgram({"Mesh.Vertex.Plain", "Mesh.Fragment.Plain"});
	whiteSolidShader = ShaderManager->getShaderProgram({"WhiteSolid.Vertex", "WhiteSolid.Fragment"});

	EventManager::get()->addListener(RESOLUTION_CHANGED_EVENT, [this](EventPtr event){ this->resolutionChanged(event); });

	camera->setNearClipDistance(0.01f);
	camera->setFarClipDistance(100.0f);
	camera->setOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
	float fovy = atanf(1.0f / 2.0f) * 2.0f;
	camera->setFOVy(fovy);
	camera->setPosition(glm::vec3(-0.5f, -0.5f, -20.0f));

	//Renderer->enableDepthTest();
	//glEnable(GL_DEPTH_TEST);
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_FRONT);


	bool renderTransparent = true;
	if (renderTransparent) {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_PixelSync);
	} else {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_Dummy);
	}
	ShaderProgramPtr transparencyShader = oitRenderer->getGatherShader();

	resolutionChanged(EventPtr());

	//std::string modelFilenamePure = "Data/Models/Monkey";
	std::string modelFilenamePure = "Data/Models/dragon";
	std::string modelFilenameOptimized = modelFilenamePure + ".binmesh";
	std::string modelFilenameObj = modelFilenamePure + ".obj";
	if (!FileUtils::get()->exists(modelFilenameOptimized)) {
		convertObjMeshToBinary(modelFilenameObj, modelFilenameOptimized);
	}
	transparentObject = parseMesh3D(modelFilenameOptimized, transparencyShader);
	transparentObject->getShaderProgram()->setUniform("color", Color(255, 255, 0, 120));
	rotation = glm::mat4(1.0f);
	scaling = glm::mat4(1.0f);
}

PixelSyncApp::~PixelSyncApp()
{
	if (videoWriter != NULL) {
		delete videoWriter;
	}
}

void PixelSyncApp::render()
{
	bool wireframe = false;

	if (videoWriter == NULL && recording) {
		videoWriter = new VideoWriter("video.mp4");
	}

	Renderer->setCamera(camera);

	//Renderer->setBlendMode(BLEND_ALPHA);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	oitRenderer->gatherBegin();
	renderScene();
	oitRenderer->gatherEnd();

	oitRenderer->renderToScreen();

	// Wireframe mode
	if (wireframe) {
		Renderer->setModelMatrix(matrixIdentity());
		Renderer->setLineWidth(1.0f);
		Renderer->enableWireframeMode();
		renderScene();
		Renderer->disableWireframeMode();
	}

	// Video recording enabled?
	if (recording) {
		videoWriter->pushWindowFrame();
	}
}

void PixelSyncApp::renderScene()
{
	Renderer->setProjectionMatrix(camera->getProjectionMatrix());
	Renderer->setViewMatrix(camera->getViewMatrix());
	//Renderer->setModelMatrix(matrixIdentity());

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	Renderer->setModelMatrix(rotation * scaling);
	//transparentObject->getShaderProgram()->setUniform("color", Color(255, 255, 0, 120));
	transparentObject->getShaderProgram()->setUniform("color", Color(165, 220, 84, 120));
	Renderer->render(transparentObject);

	Renderer->setModelMatrix(matrixTranslation(glm::vec3(0.5f,0.0f,-4.0f)));
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	transparentObject->getShaderProgram()->setUniform("color", Color(0, 255, 128, 120));
	//Renderer->render(transparentObject);
}

void PixelSyncApp::resolutionChanged(EventPtr event)
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();
	glViewport(0, 0, width, height);

	camera->onResolutionChanged(event);
	camera->onResolutionChanged(event);
	oitRenderer->resolutionChanged();
}

void PixelSyncApp::update(float dt)
{
	AppLogic::update(dt);

	const float ROT_SPEED = 0.001f;

	// Rotate object around its origin
	if (Keyboard->isKeyDown(SDLK_x)) {
		glm::quat rot = glm::quat(glm::vec3(dt*ROT_SPEED, 0.0f, 0.0f));
		camera->rotate(rot);
	}
	if (Keyboard->isKeyDown(SDLK_y)) {
		glm::quat rot = glm::quat(glm::vec3(0.0f, dt*ROT_SPEED, 0.0f));
		camera->rotate(rot);
	}
	if (Keyboard->isKeyDown(SDLK_z)) {
		glm::quat rot = glm::quat(glm::vec3(0.0f, 0.0f, dt*ROT_SPEED));
		camera->rotate(rot);
	}

	const float MOVE_SPEED = 0.005f;

	if (Keyboard->isKeyDown(SDLK_PAGEDOWN)) {
		camera->translate(glm::vec3(0.0f, dt*MOVE_SPEED, 0.0f));
	}
	if (Keyboard->isKeyDown(SDLK_PAGEUP)) {
		camera->translate(glm::vec3(0.0f, -dt*MOVE_SPEED, 0.0f));
	}
	if (Keyboard->isKeyDown(SDLK_DOWN)) {
		camera->translate(glm::vec3(0.0f, 0.0f, -dt*MOVE_SPEED));
	}
	if (Keyboard->isKeyDown(SDLK_UP)) {
		camera->translate(glm::vec3(0.0f, 0.0f, +dt*MOVE_SPEED));
	}
	if (Keyboard->isKeyDown(SDLK_LEFT)) {
		camera->translate(glm::vec3(dt*MOVE_SPEED, 0.0f, 0.0f));
	}
	if (Keyboard->isKeyDown(SDLK_RIGHT)) {
		camera->translate(glm::vec3(-dt*MOVE_SPEED, 0.0f, 0.0f));
	}

	// Zoom in/out
	if (Mouse->getScrollWheel() > 0.1 || Mouse->getScrollWheel() < 0.1) {
		camera->scale((1+Mouse->getScrollWheel()*dt*0.5f)*glm::vec3(1.0,1.0,1.0));
	}
}
