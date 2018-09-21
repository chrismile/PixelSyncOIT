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
#include <boost/algorithm/string/predicate.hpp>

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
#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/Shader/ShaderManager.hpp>
#include <Graphics/Texture/TextureManager.hpp>
#include <ImGui/ImGuiWrapper.hpp>

#include "Utils/MeshSerializer.hpp"
#include "Utils/OBJLoader.hpp"
#include "Utils/TrajectoryLoader.hpp"
#include "OIT/OIT_Dummy.hpp"
#include "OIT/OIT_KBuffer.hpp"
#include "OIT/OIT_LinkedList.hpp"
#include "OIT/OIT_MLAB.hpp"
#include "OIT/OIT_HT.hpp"
#include "OIT/OIT_MBOIT.hpp"
#include "OIT/OIT_DepthComplexity.hpp"
#include "MainApp.hpp"

void openglErrorCallback()
{
    std::cerr << "Application callback" << std::endl;
}

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
	//camera->setPosition(glm::vec3(-0.5f, -0.5f, -20.0f));
	camera->setPosition(glm::vec3(-0.0f, 0.1f, -2.4f));

	bandingColor = Color(165, 220, 84, 120);

	//Renderer->enableDepthTest();
	//glEnable(GL_DEPTH_TEST);
	if (cullBackface) {
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
	} else {
		glCullFace(GL_BACK);
		glDisable(GL_CULL_FACE);
	}
	Renderer->setErrorCallback(&openglErrorCallback);
	Renderer->setDebugVerbosity(DEBUG_OUTPUT_CRITICAL_ONLY);

	setRenderMode(mode, true);
	loadModel(MODEL_FILENAMES[usedModelIndex]);
}


void PixelSyncApp::resolutionChanged(EventPtr event)
{
	Window *window = AppSettings::get()->getMainWindow();
	int width = window->getWidth();
	int height = window->getHeight();
	glViewport(0, 0, width, height);

	// Buffers for off-screen rendering
	sceneFramebuffer = Renderer->createFBO();
	TextureSettings textureSettings;
	textureSettings.internalFormat = GL_RGBA;
	sceneTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
	sceneFramebuffer->bindTexture(sceneTexture);
	sceneDepthRBO = Renderer->createRBO(width, height, DEPTH24_STENCIL8);
	sceneFramebuffer->bindRenderbuffer(sceneDepthRBO, DEPTH_STENCIL_ATTACHMENT);


	camera->onResolutionChanged(event);
	camera->onResolutionChanged(event);
	oitRenderer->resolutionChanged(sceneFramebuffer, sceneDepthRBO);
	reRender = true;
}


#include <Graphics/Window.hpp>
#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/Texture/TextureManager.hpp>

void test()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    FramebufferObjectPtr gBufferFBO = Renderer->createFBO();

    TextureSettings textureSettings;
    textureSettings.internalFormat = GL_RGB16F;
    TexturePtr positionTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    gBufferFBO->bindTexture(positionTexture, COLOR_ATTACHMENT0);

    textureSettings.internalFormat = GL_RGB16F;
    TexturePtr normalTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    gBufferFBO->bindTexture(normalTexture, COLOR_ATTACHMENT1);

    RenderbufferObjectPtr depthStencilRBO = Renderer->createRBO(width, height, DEPTH24_STENCIL8);
    gBufferFBO->bindRenderbuffer(depthStencilRBO, DEPTH_STENCIL_ATTACHMENT);

    Renderer->bindFBO(gBufferFBO);
    // Render scene


    ShaderProgramPtr ssaoPhongShader = ShaderManager->getShaderProgram({"Mesh.Vertex.Plain", "Mesh.Fragment.Plain"});
    ssaoPhongShader->setUniform("gPositionTexture", positionTexture, 0);
    ssaoPhongShader->setUniform("gNormalTexture", normalTexture, 1);
    // Render full screen squad
}

void depthPeeling()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    FramebufferObjectPtr depthPeelingFBO = Renderer->createFBO();

    TextureSettings textureSettings;
    textureSettings.internalFormat = GL_RGBA;
    TexturePtr colorTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    TexturePtr colorTextureAccumulator = TextureManager->createEmptyTexture(width, height, textureSettings);
    depthPeelingFBO->bindTexture(colorTexture, COLOR_ATTACHMENT0);

    textureSettings.internalFormat = GL_R32F;
    TexturePtr depthTexture0 = TextureManager->createEmptyTexture(width, height, textureSettings);
    TexturePtr depthTexture1 = TextureManager->createEmptyTexture(width, height, textureSettings);
    depthPeelingFBO->bindTexture(depthTexture0, DEPTH_ATTACHMENT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_GREATER);
    glDepthFunc(GL_LESS);
}

void PixelSyncApp::loadModel(const std::string &filename)
{
	// Pure filename without extension (to create compressed .binmesh filename)
	modelFilenamePure = FileUtils::get()->removeExtension(filename);

	std::string modelFilenameOptimized = modelFilenamePure + ".binmesh";
	std::string modelFilenameObj = modelFilenamePure + ".obj";
	if (!FileUtils::get()->exists(modelFilenameOptimized)) {
		if (boost::starts_with(modelFilenamePure, "Data/Models")) {
			convertObjMeshToBinary(modelFilenameObj, modelFilenameOptimized);
		} else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
			convertObjTrajectoryDataToBinaryMesh(modelFilenameObj, modelFilenameOptimized);
		}
	}
	transparentObject = parseMesh3D(modelFilenameOptimized, transparencyShader);
	boundingBox = transparentObject.boundingBox;
	rotation = glm::mat4(1.0f);
	scaling = glm::mat4(1.0f);

	camera->setOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
	camera->setScale(glm::vec3(1.0f));
	if (modelFilenamePure == "Data/Models/Ship_04") {
		transparencyShader->setUniform("bandedColorShading", 0);
		camera->setPosition(glm::vec3(0.0f, -1.5f, -5.0f));
	} else {
		transparencyShader->setUniform("bandedColorShading", 1);
		if (modelFilenamePure == "Data/Models/dragon") {
			camera->setPosition(glm::vec3(-0.15f, -0.8f, -2.4f));
			const float scalingFactor = 0.2f;
			scaling = matrixScaling(glm::vec3(scalingFactor));
			//boundingSphere.center *= scalingFactor;
			//boundingSphere.radius *= scalingFactor;
		} else {
			camera->setPosition(glm::vec3(-0.0f, 0.1f, -2.4f));
		}
	}

	boundingBox = boundingBox.transformed(rotation * scaling);
	reRender = true;
}

void PixelSyncApp::setRenderMode(RenderModeOIT newMode, bool forceReset)
{
	if (mode == newMode && !forceReset) {
		return;
	}

	reRender = true;
	ShaderManager->invalidateShaderCache();

	mode = newMode;
	if (mode == RENDER_MODE_OIT_KBUFFER) {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_KBuffer);
	} else if (mode == RENDER_MODE_OIT_LINKED_LIST) {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_LinkedList);
	} else if (mode == RENDER_MODE_OIT_MLAB) {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_MLAB);
	} else if (mode == RENDER_MODE_OIT_HT) {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_HT);
    } else if (mode == RENDER_MODE_OIT_MBOIT) {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_MBOIT);
    } else if (mode == RENDER_MODE_OIT_DEPTH_COMPLEXITY) {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_DepthComplexity);
	} else if (mode == RENDER_MODE_OIT_DUMMY) {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_Dummy);
	} else {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_Dummy);
		Logfile::get()->writeError("PixelSyncApp::setRenderMode: Invalid mode.");
		mode = RENDER_MODE_OIT_DUMMY;
	}
	oitRenderer->setRenderSceneFunction([this]() { this->renderScene(); });

	transparencyShader = oitRenderer->getGatherShader();

	/*if (mode == RENDER_MODE_OIT_DEPTH_COMPLEXITY) {
		Renderer->setDebugVerbosity(DEBUG_OUTPUT_CRITICAL_ONLY);
	} else {
		Renderer->setDebugVerbosity(DEBUG_OUTPUT_MEDIUM_AND_ABOVE);
	}*/

	if (transparentObject.isLoaded()) {
		transparentObject.setNewShader(transparencyShader);
		if (modelFilenamePure == "Data/Models/Ship_04") {
			transparencyShader->setUniform("bandedColorShading", 0);
		} else {
			transparencyShader->setUniform("bandedColorShading", 1);
		}
	}
	//Timer->setFPSLimit(false, 60);
	Timer->setFPSLimit(true, 60);

    resolutionChanged(EventPtr());
}

PixelSyncApp::~PixelSyncApp()
{
	timer.printTimeMS("gatherBegin");
	timer.printTimeMS("renderScene");
	timer.printTimeMS("gatherEnd");
	timer.printTimeMS("renderToScreen");
	timer.printTotalAvgTime();
	timer.deleteAll();

	if (videoWriter != NULL) {
		delete videoWriter;
	}
}

#define PROFILING_MODE

void PixelSyncApp::render()
{
	if (videoWriter == NULL && recording) {
		videoWriter = new VideoWriter("video.mp4");
	}


	/*glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_LESS);
	//glDepthMask(GL_FALSE);

		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF);
		glClear(GL_STENCIL_BUFFER_BIT);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);

	glStencilFunc(GL_EQUAL, 1, 0xFF);
	glStencilMask(0x00);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glDepthMask(GL_TRUE);*/

	//Timer->sleepMilliseconds(4000);

	reRender = reRender || oitRenderer->needsReRender();
	/*if (oitRenderer->needsNewShader()) {
		transparencyShader = oitRenderer->getGatherShader();
		if (transparentObject.isLoaded()) {
			transparentObject.setNewShader(transparencyShader);
			if (modelFilenamePure == "Data/Models/Ship_04") {
				transparencyShader->setUniform("bandedColorShading", 0);
			} else {
				transparencyShader->setUniform("bandedColorShading", 1);
			}
		}
		transparentObject.setNewShader(transparencyShader);
	}*/

	if (continuousRendering || reRender) {
		renderOIT();
		reRender = false;
		Renderer->unbindFBO();
	}

	// Render to screen
	Renderer->setProjectionMatrix(matrixIdentity());
	Renderer->setViewMatrix(matrixIdentity());
	Renderer->setModelMatrix(matrixIdentity());
	Renderer->blitTexture(sceneTexture, AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)));

	renderGUI();

	// Video recording enabled?
	if (recording) {
		videoWriter->pushWindowFrame();
	}
}


void PixelSyncApp::renderOIT()
{
	bool wireframe = false;

	if (mode == RENDER_MODE_OIT_MBOIT) {
		AABB3 screenSpaceBoundingBox = boundingBox.transformed(camera->getViewMatrix());
		static_cast<OIT_MBOIT*>(oitRenderer.get())->setScreenSpaceBoundingBox(screenSpaceBoundingBox);
	}

	//Renderer->setBlendMode(BLEND_ALPHA);

	Renderer->bindFBO(sceneFramebuffer);
	Renderer->clearFramebuffer();
	//Renderer->setCamera(camera); // Resets rendertarget...

	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);

#ifdef PROFILING_MODE
	timer.start("gatherBegin");
	oitRenderer->gatherBegin();
	timer.end();

	timer.start("renderScene");
	oitRenderer->renderScene();
	timer.end();

	timer.start("gatherEnd");
	oitRenderer->gatherEnd();
	timer.end();

	timer.start("renderToScreen");
	oitRenderer->renderToScreen();
	timer.end();
#else
	oitRenderer->gatherBegin();
	renderScene();
	oitRenderer->gatherEnd();

	oitRenderer->renderToScreen();
#endif

	// Wireframe mode
	if (wireframe) {
		Renderer->setModelMatrix(matrixIdentity());
		Renderer->setLineWidth(1.0f);
		Renderer->enableWireframeMode();
		renderScene();
		Renderer->disableWireframeMode();
	}
}


void PixelSyncApp::renderGUI()
{
	ImGuiWrapper::get()->renderStart();
    //ImGuiWrapper::get()->renderDemoWindow();

    if (showSettingsWindow) {
        ImGui::Begin("Settings", &showSettingsWindow);

        // FPS
        static float displayFPS = 60.0f;
        static uint64_t fpsCounter = 0;
        if (Timer->getTicksMicroseconds() - fpsCounter > 1e6) {
			displayFPS = ImGui::GetIO().Framerate;
			fpsCounter = Timer->getTicksMicroseconds();
        }
        // TODO: Average over multiple frames?
		//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / displayFPS, displayFPS);
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / fps, fps);
		ImGui::Separator();

		// Mode selection of OIT algorithms
		bool updateMode = false;
		for (int i = 0; i < NUM_OIT_MODES; i++) {
			if (ImGui::RadioButton(OIT_MODE_NAMES[i], (int*)&mode, i)) { updateMode = true; }
			if (i != NUM_OIT_MODES-1 && i != 3) { ImGui::SameLine(); }
		}
		ImGui::Separator();
		if (updateMode) {
			setRenderMode(mode, true);
		}

		// Selection of displayed model
		updateMode = false;
		for (int i = 0; i < NUM_MODELS; i++) {
			if (ImGui::RadioButton(MODEL_DISPLAYNAMES[i], &usedModelIndex, i)) { updateMode = true; }
			if (i != NUM_MODELS-1 && i != 2) { ImGui::SameLine(); }
		}
		ImGui::Separator();
		if (updateMode) {
			loadModel(MODEL_FILENAMES[usedModelIndex]);
		}

		// Color selection in binning mode (if not showing all values in different color channels in mode 1)
        if (modelFilenamePure != "Data/Models/Ship_04") {
            static ImVec4 colorSelection = ImColor(165, 220, 84, 120);
            int misc_flags = 0;
            ImGui::Text("Color widget:");
            ImGui::SameLine(); ImGuiWrapper::get()->showHelpMarker("Click on the colored square to open a color picker."
                                                                   "\nCTRL+click on individual component to input value.\n");
            if (ImGui::ColorEdit4("MyColor##1", (float*)&colorSelection, misc_flags)) {
                bandingColor = colorFromFloat(colorSelection.x, colorSelection.y, colorSelection.z, colorSelection.w);
				reRender = true;
			}
            ImGui::Separator();
        }

        if (ImGui::Checkbox("Cull back face", &cullBackface)) {
            if (cullBackface) {
                glEnable(GL_CULL_FACE);
            } else {
                glDisable(GL_CULL_FACE);
            }
            reRender = true;
        }

        oitRenderer->renderGUI();

		//windowActive = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::End();
    }

	/*ImGui::Begin("Settings");

	static float wrap_width = 200.0f;
	ImGui::SliderFloat("Wrap width", &wrap_width, -20, 600, "%.0f");

	ImGui::Text("Color widget:");
	ImGui::SameLine(); ShowHelpMarker("Click on the colored square to open a color picker.\nCTRL+click on individual component to input value.\n");
	ImGui::ColorEdit3("MyColor##1", (float*)&color, misc_flags);

	ImGui::End();*/

	//ImGui::RadioButton
	ImGuiWrapper::get()->renderEnd();
}

void PixelSyncApp::renderScene()
{
    ShaderProgramPtr transparencyShader = oitRenderer->getGatherShader();

    if (mode == RENDER_MODE_OIT_MBOIT) {
        // Hack for supporting multiple passes...
        if (modelFilenamePure == "Data/Models/Ship_04") {
            transparencyShader->setUniform("bandedColorShading", 0);
        } else {
            transparencyShader->setUniform("bandedColorShading", 1);
        }
    }

	Renderer->setProjectionMatrix(camera->getProjectionMatrix());
	Renderer->setViewMatrix(camera->getViewMatrix());
	//Renderer->setModelMatrix(matrixIdentity());

	Renderer->setModelMatrix(rotation * scaling);
    transparencyShader->setUniform("color", bandingColor);
    transparentObject.render(transparencyShader);
}


void PixelSyncApp::processSDLEvent(const SDL_Event &event)
{
	ImGuiWrapper::get()->processSDLEvent(event);
}


void PixelSyncApp::update(float dt)
{
	AppLogic::update(dt);
	//dt = 1/60.0f;

	//std::cout << "dt: " << dt << std::endl;

	//std::cout << ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) << std::endl;
	//std::cout << ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) << std::endl;


	ImGuiIO &io = ImGui::GetIO();
	if (io.WantCaptureKeyboard) {
		// Ignore inputs below
		return;
	}


	if (Keyboard->keyPressed(SDLK_0)) {
        setRenderMode((RenderModeOIT)0);
	} else if (Keyboard->keyPressed(SDLK_1)) {
        setRenderMode((RenderModeOIT)1);
	} else if (Keyboard->keyPressed(SDLK_2)) {
        setRenderMode((RenderModeOIT)2);
	} else if (Keyboard->keyPressed(SDLK_3)) {
        setRenderMode((RenderModeOIT)3);
	} else if (Keyboard->keyPressed(SDLK_4)) {
        setRenderMode((RenderModeOIT)4);
	} else if (Keyboard->keyPressed(SDLK_5)) {
		setRenderMode((RenderModeOIT)5);
	} else if (Keyboard->keyPressed(SDLK_6)) {
		setRenderMode((RenderModeOIT)6);
	}

	const float ROT_SPEED = 1.0f;

	// Rotate scene around camera origin
	if (Keyboard->isKeyDown(SDLK_x)) {
		glm::quat rot = glm::quat(glm::vec3(dt*ROT_SPEED, 0.0f, 0.0f));
		camera->rotate(rot);
		reRender = true;
	}
	if (Keyboard->isKeyDown(SDLK_y)) {
		glm::quat rot = glm::quat(glm::vec3(0.0f, dt*ROT_SPEED, 0.0f));
		camera->rotate(rot);
		reRender = true;
	}
	if (Keyboard->isKeyDown(SDLK_z)) {
		glm::quat rot = glm::quat(glm::vec3(0.0f, 0.0f, dt*ROT_SPEED));
		camera->rotate(rot);
		reRender = true;
	}

	const float MOVE_SPEED = 1.0f;


	glm::mat4 rotationMatrix = glm::mat4(camera->getOrientation());
    glm::mat4 invRotationMatrix = glm::inverse(rotationMatrix);
	if (Keyboard->isKeyDown(SDLK_PAGEDOWN)) {
		camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, dt*MOVE_SPEED, 0.0f)));
		reRender = true;
	}
	if (Keyboard->isKeyDown(SDLK_PAGEUP)) {
		camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, -dt*MOVE_SPEED, 0.0f)));
		reRender = true;
	}
	if (Keyboard->isKeyDown(SDLK_DOWN) || Keyboard->isKeyDown(SDLK_s)) {
		camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, 0.0f, -dt*MOVE_SPEED)));
		reRender = true;
	}
	if (Keyboard->isKeyDown(SDLK_UP) || Keyboard->isKeyDown(SDLK_w)) {
		camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, 0.0f, +dt*MOVE_SPEED)));
		reRender = true;
	}
	if (Keyboard->isKeyDown(SDLK_LEFT) || Keyboard->isKeyDown(SDLK_a)) {
		camera->translate(transformPoint(invRotationMatrix, glm::vec3(dt*MOVE_SPEED, 0.0f, 0.0f)));
		reRender = true;
	}
	if (Keyboard->isKeyDown(SDLK_RIGHT) || Keyboard->isKeyDown(SDLK_d)) {
		camera->translate(transformPoint(invRotationMatrix, glm::vec3(-dt*MOVE_SPEED, 0.0f, 0.0f)));
		reRender = true;
	}

	if (io.WantCaptureMouse) {
		// Ignore inputs below
		return;
	}

	// Zoom in/out
	if (Mouse->getScrollWheel() > 0.1 || Mouse->getScrollWheel() < -0.1) {
		camera->scale((1+Mouse->getScrollWheel()*dt*0.5f)*glm::vec3(1.0,1.0,1.0));
		reRender = true;
	}

    const float MOUSE_ROT_SPEED = 0.05f;

    // Mouse rotation
	if (Mouse->isButtonDown(1) && Mouse->mouseMoved()) {
	    sgl::Point2 pixelMovement = Mouse->mouseMovement();
        float yaw = dt*MOUSE_ROT_SPEED*pixelMovement.x;
        float pitch = dt*MOUSE_ROT_SPEED*pixelMovement.y;

        glm::quat rotYaw = glm::quat(glm::vec3(0.0f, yaw, 0.0f));
        glm::quat rotPitch = glm::quat(pitch*glm::vec3(rotationMatrix[0][0], rotationMatrix[1][0], rotationMatrix[2][0]));
        //glm::quat rot = glm::quat(glm::vec3(pitch, yaw, 0.0f));
        camera->rotate(rotYaw*rotPitch);
		reRender = true;
	}
}
