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
#include <Input/Mouse.hpp>
#include <Input/Keyboard.hpp>
#include <Utils/File/Logfile.hpp>
#include <Utils/File/FileUtils.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/Shader/ShaderManager.hpp>

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
	camera->setOrientation(glm::quat());
	float fovy = atanf(1.0f / 2.0f) * 2.0f;
	camera->setFOVy(fovy);
	camera->setPosition(glm::vec3(-0.5f, -0.5f, -5.0f));

	//Renderer->enableDepthTest(); TODO
	//glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	//glCullFace(GL_FRONT);

	resolutionChanged(EventPtr());

	bool renderTransparent = true;
	if (renderTransparent) {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_PixelSync);
	} else {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_Dummy);
	}
	ShaderProgramPtr transparencyShader = oitRenderer->getGatherShader();

	transparentObject = parseObjMesh("Data/Models/Monkey.obj", transparencyShader);
	//transparentObject = parseObjMesh("Data/Models/Cube.obj", transparencyShader);
	transparentObject->getShaderProgram()->setUniform("color", Color(255, 255, 0));
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
	Renderer->setModelMatrix(rotation * scaling);

	Renderer->render(transparentObject);
}

void PixelSyncApp::resolutionChanged(EventPtr event)
{
	camera->onResolutionChanged(event);
	camera->onResolutionChanged(event);
}

void PixelSyncApp::update(float dt)
{
	AppLogic::update(dt);

	// Rotate object around its origin
	if (Keyboard->isKeyDown(SDLK_x)) {
		rotation = rotation*glm::eulerAngleYXZ(dt*0.001f, 0.0f, 0.0f);
	}
	if (Keyboard->isKeyDown(SDLK_y)) {
		rotation = rotation*glm::eulerAngleYXZ(0.0f, dt*0.001f, 0.0f);
	}
	if (Keyboard->isKeyDown(SDLK_z)) {
		rotation = rotation*glm::eulerAngleYXZ(0.0f, 0.0f, dt*0.001f);
	}

	// Zoom in/out
	if (Mouse->getScrollWheel() > 0.1 || Mouse->getScrollWheel() < 0.1) {
		scaling = scaling*glm::scale((1+Mouse->getScrollWheel()*dt*0.005f)*glm::vec3(1.0,1.0,1.0));
	}
}
