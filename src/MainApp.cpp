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
#include <Graphics/Texture/Bitmap.hpp>
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
#include "OIT/OIT_DepthPeeling.hpp"
#include "OIT/TilingMode.hpp"
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
	clearColor = Color(0, 0, 0, 255);

	fpsArray.resize(16, 60.0f);
	framerateSmoother = FramerateSmoother(1);

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

	if (useSSAO) {
		ShaderManager->addPreprocessorDefine("USE_SSAO", "");
	}

	setRenderMode(mode, true);
	loadModel(MODEL_FILENAMES[usedModelIndex]);


    if (perfMeasurementMode) {
        measurer = new AutoPerfMeasurer(getAllTestModes(), "performance.csv",
                                        [this](const InternalState &newState) { this->setNewState(newState); });
        continuousRendering = true; // Always use continuous rendering in performance measurement mode
    } else {
        measurer = NULL;
    }
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
	ssaoHelper.resolutionChanged();
	reRender = true;
}

void PixelSyncApp::saveScreenshot(const std::string &filename)
{
	if (uiOnScreenshot) {
		AppLogic::saveScreenshot(filename);
	} else {
		Window *window = AppSettings::get()->getMainWindow();
		int width = window->getWidth();
		int height = window->getHeight();

		Renderer->bindFBO(sceneFramebuffer);
		BitmapPtr bitmap(new Bitmap(width, height, 32));
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->getPixels());
		bitmap->savePNG(filename.c_str(), true);
		Renderer->unbindFBO();
	}
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

	updateShaderMode(SHADER_MODE_UPDATE_NEW_MODEL);

	transparentObject = parseMesh3D(modelFilenameOptimized, transparencyShader);
	boundingBox = transparentObject.boundingBox;
	rotation = glm::mat4(1.0f);
	scaling = glm::mat4(1.0f);

	// Set position & banding mode dependent on the model
	camera->setOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
	camera->setScale(glm::vec3(1.0f));
	if (modelFilenamePure == "Data/Models/Ship_04") {
		transparencyShader->setUniform("bandedColorShading", 0);
		camera->setPosition(glm::vec3(0.0f, -1.5f, -5.0f));
	} else {
		if (shaderMode != SHADER_MODE_VORTICITY) {
			transparencyShader->setUniform("bandedColorShading", 1);
		}

		if (modelFilenamePure == "Data/Models/dragon") {
			camera->setPosition(glm::vec3(-0.15f, -0.8f, -2.4f));
			const float scalingFactor = 0.2f;
			scaling = matrixScaling(glm::vec3(scalingFactor));
		} else if (boost::starts_with(modelFilenamePure, "Data/Trajectories/lagranto")) {
			camera->setPosition(glm::vec3(-0.6f, -0.0f, -8.8f));
		}  else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
			camera->setPosition(glm::vec3(-0.6f, -0.4f, -1.8f));
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
	oitRenderer = boost::shared_ptr<OIT_Renderer>();
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
    } else if (mode == RENDER_MODE_OIT_DEPTH_PEELING) {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_DepthPeeling);
    } else {
		oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_Dummy);
		Logfile::get()->writeError("PixelSyncApp::setRenderMode: Invalid mode.");
		mode = RENDER_MODE_OIT_DUMMY;
	}
	oitRenderer->setRenderSceneFunction([this]() { this->renderScene(); });

	updateShaderMode(SHADER_MODE_UPDATE_NEW_OIT_RENDERER);

	transparencyShader = oitRenderer->getGatherShader();

	if (transparentObject.isLoaded()) {
		transparentObject.setNewShader(transparencyShader);
		if (shaderMode != SHADER_MODE_VORTICITY) {
			if (modelFilenamePure == "Data/Models/Ship_04") {
				transparencyShader->setUniform("bandedColorShading", 0);
			} else {
				transparencyShader->setUniform("bandedColorShading", 1);
			}
		}
	}
	//Timer->setFPSLimit(false, 60);
	Timer->setFPSLimit(true, 60);

    resolutionChanged(EventPtr());
}

void PixelSyncApp::updateShaderMode(ShaderModeUpdate modeUpdate)
{
	if (boost::starts_with(modelFilenamePure, "Data/Trajectories/")) {
		if (shaderMode != SHADER_MODE_VORTICITY || modeUpdate == SHADER_MODE_UPDATE_NEW_OIT_RENDERER
				|| modeUpdate == SHADER_MODE_UPDATE_SSAO_CHANGE) {
			oitRenderer->setGatherShader("PseudoPhongVorticity");
			transparencyShader = oitRenderer->getGatherShader();
			shaderMode = SHADER_MODE_VORTICITY;
		}
	} else {
		if (shaderMode == SHADER_MODE_VORTICITY || modeUpdate == SHADER_MODE_UPDATE_SSAO_CHANGE) {
			oitRenderer->setGatherShader("PseudoPhong");
			transparencyShader = oitRenderer->getGatherShader();
			shaderMode = SHADER_MODE_PSEUDO_PHONG;
		}
	}
}

void PixelSyncApp::setNewState(const InternalState &newState)
{
	// 1. Handle global state changes like SSAO, tiling mode
	/*setNewTilingMode(newState.tilingWidth, newState.tilingHeight);
	if (useSSAO != newState.useSSAO) {
		useSSAO = newState.useSSAO;
		ShaderManager->invalidateShaderCache();
		if (useSSAO) {
			ShaderManager->addPreprocessorDefine("USE_SSAO", "");
		} else {
			ShaderManager->removePreprocessorDefine("USE_SSAO");
		}
		updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
	}

	// 2. Load right model file
	std::string modelFilename = "";
	for (int i = 0; i < NUM_MODELS; i++) {
		if (MODEL_DISPLAYNAMES[i] == newState.modelName) {
			modelFilename = MODEL_FILENAMES[i];
            usedModelIndex = i;
		}
	}

	if (modelFilename.length() < 1) {
		Logfile::get()->writeError(std::string() + "Error in PixelSyncApp::setNewState: Invalid model name \""
								   + "\".");
		exit(1);
	}
	loadModel(modelFilename);*/

	// 3. Set OIT algorithm
	setRenderMode(newState.oitAlgorithm, true);

	// 4. Pass state change to OIT mode to handle internally necessary state changes.
	/*if (firstState || lastState.oitAlgorithmSettings.getMap() != newState.oitAlgorithmSettings.getMap()
	        || lastState.tilingWidth != newState.tilingWidth || lastState.tilingHeight != newState.tilingHeight
            || lastState.useStencilBuffer != newState.useStencilBuffer) {
        oitRenderer->setNewState(newState);
	}*/

	lastState = newState;
	firstState = false;
}



PixelSyncApp::~PixelSyncApp()
{
#ifdef PROFILING_MODE
	timer.printTimeMS("gatherBegin");
	timer.printTimeMS("renderScene");
	timer.printTimeMS("gatherEnd");
	timer.printTimeMS("renderToScreen");
	timer.printTotalAvgTime();
	timer.deleteAll();
#endif

	if (perfMeasurementMode) {
		delete measurer;
		measurer = NULL;
	}

	if (videoWriter != NULL) {
		delete videoWriter;
	}
}

void PixelSyncApp::render()
{
	if (videoWriter == NULL && recording) {
		videoWriter = new VideoWriter("video.mp4");
	}


	reRender = reRender || oitRenderer->needsReRender();

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
		static_cast<OIT_MBOIT*>(oitRenderer.get())->setScreenSpaceBoundingBox(screenSpaceBoundingBox, camera);
	}

	//Renderer->setBlendMode(BLEND_ALPHA);

	if (useSSAO) {
		ssaoHelper.preRender([this]() { this->renderScene(); });
	}

	Renderer->bindFBO(sceneFramebuffer);
	Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, clearColor);
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
	if (perfMeasurementMode) {
		measurer->startMeasure();
	}

	oitRenderer->gatherBegin();
	renderScene();
	oitRenderer->gatherEnd();

	oitRenderer->renderToScreen();

	if (perfMeasurementMode) {
		measurer->endMeasure();
	}
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
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / fps, fps);
		ImGui::Separator();

		// Mode selection of OIT algorithms
		if (ImGui::Combo("OIT Mode", (int*)&mode, OIT_MODE_NAMES, IM_ARRAYSIZE(OIT_MODE_NAMES))) {
			setRenderMode(mode, true);
		}
		ImGui::Separator();

		// Selection of displayed model
		if (ImGui::Combo("Model Name", &usedModelIndex, MODEL_DISPLAYNAMES, IM_ARRAYSIZE(MODEL_DISPLAYNAMES))) {
			loadModel(MODEL_FILENAMES[usedModelIndex]);
		}
		ImGui::Separator();

		static bool showSceneSettings = true;
		if (ImGui::CollapsingHeader("Scene Settings", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
			renderSceneSettingsGUI();
		}
		/*if (ImGui::TreeNode("Scene Settings")) {
			renderSceneSettingsGUI();
			ImGui::TreePop();
		}*/

        oitRenderer->renderGUI();

		//windowActive = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::End();
    }

	ImGuiWrapper::get()->renderEnd();
}

void PixelSyncApp::renderSceneSettingsGUI()
{
	// Color selection in binning mode (if not showing all values in different color channels in mode 1)
	static ImVec4 colorSelection = ImColor(165, 220, 84, 120);
	if (modelFilenamePure != "Data/Models/Ship_04" && mode != RENDER_MODE_OIT_DEPTH_COMPLEXITY) {
		int misc_flags = 0;
		if (ImGui::ColorEdit4("Model Color", (float*)&colorSelection, misc_flags)) {
			bandingColor = colorFromFloat(colorSelection.x, colorSelection.y, colorSelection.z, colorSelection.w);
			reRender = true;
		}
		/*ImGui::SameLine();
        ImGuiWrapper::get()->showHelpMarker("Click on the colored square to open a color picker."
                                            "\nCTRL+click on individual component to input value.\n");*/
	} else if (mode != RENDER_MODE_OIT_DEPTH_COMPLEXITY) {
		if (ImGui::SliderFloat("Opacity", &colorSelection.w, 0.0f, 1.0f, "%.2f")) {
			bandingColor = colorFromFloat(colorSelection.x, colorSelection.y, colorSelection.z, colorSelection.w);
			reRender = true;
		}
	}
	//ImGui::Separator();

	static ImVec4 clearColorSelection = ImColor(0, 0, 0, 255);
	if (ImGui::ColorEdit3("Clear Color", (float*)&clearColorSelection, 0)) {
		clearColor = colorFromFloat(clearColorSelection.x, clearColorSelection.y, clearColorSelection.z,
									clearColorSelection.w);
		reRender = true;
	}

	// Select light direction
	// Spherical coordinates: (r, θ, φ), i.e. with radial distance r, azimuthal angle θ (theta), and polar angle φ (phi)
	static float theta = sgl::PI/2;
	static float phi = 0.0f;
	bool angleChanged = false;
	angleChanged = ImGui::SliderAngle("Light Azimuth", &theta, 0.0f) || angleChanged;
	angleChanged = ImGui::SliderAngle("Light Polar Angle", &phi, 0.0f) || angleChanged;
	if (angleChanged) {
		// https://en.wikipedia.org/wiki/List_of_common_coordinate_transformations#To_cartesian_coordinates
		lightDirection = glm::vec3(sinf(theta) * cosf(phi), sinf(theta) * sinf(phi), cosf(theta));
		reRender = true;
	}

	// FPS
	//ImGui::PlotLines("Frame Times", &fpsArray.front(), fpsArray.size(), fpsArrayOffset);

	if (ImGui::Checkbox("Cull back face", &cullBackface)) {
		if (cullBackface) {
			glEnable(GL_CULL_FACE);
		} else {
			glDisable(GL_CULL_FACE);
		}
		reRender = true;
	} ImGui::SameLine();
	ImGui::Checkbox("Continuous rendering", &continuousRendering);
	ImGui::Checkbox("UI on Screenshot", &uiOnScreenshot);ImGui::SameLine();

	if (ImGui::Checkbox("SSAO", &useSSAO)) {
		ShaderManager->invalidateShaderCache();
		if (useSSAO) {
			ShaderManager->addPreprocessorDefine("USE_SSAO", "");
		} else {
			ShaderManager->removePreprocessorDefine("USE_SSAO");
		}
		updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
		reRender = true;
	}

	ImGui::SliderFloat("Move Speed", &MOVE_SPEED, 0.1, 1.0);

	//ImGui::Separator();
}

sgl::ShaderProgramPtr PixelSyncApp::setUniformValues()
{
	ShaderProgramPtr transparencyShader;

	if (!useSSAO || !ssaoHelper.isPreRenderPass()) {
		transparencyShader = oitRenderer->getGatherShader();
		if (shaderMode == SHADER_MODE_VORTICITY) {
			transparencyShader->setUniform("minVorticity", 0.0f);
			transparencyShader->setUniform("maxVorticity", 1.0f);
		}

		if (shaderMode != SHADER_MODE_VORTICITY) {
			// Hack for supporting multiple passes...
			if (modelFilenamePure == "Data/Models/Ship_04") {
				transparencyShader->setUniform("bandedColorShading", 0);
			} else {
				transparencyShader->setUniform("bandedColorShading", 1);
			}
		}

		transparencyShader->setUniform("color", bandingColor);
		transparencyShader->setUniform("lightDirection", lightDirection);
	}

	if (useSSAO) {
		if (ssaoHelper.isPreRenderPass()) {
			transparencyShader = ssaoHelper.getGeometryPassShader();
		} else {
			TexturePtr ssaoTexture = ssaoHelper.getSSAOTexture();
			transparencyShader->setUniform("ssaoTexture", ssaoTexture, 4);
		}
	}

	return transparencyShader;
}

void PixelSyncApp::renderScene()
{
	ShaderProgramPtr transparencyShader;
	if (!boost::starts_with(oitRenderer->getGatherShader()->getShaderList().front()->getFileID(),
			"DepthPeelingGatherDepthComplexity")) {
		transparencyShader = setUniformValues();
	} else {
		transparencyShader = oitRenderer->getGatherShader();
	}

	Renderer->setProjectionMatrix(camera->getProjectionMatrix());
	Renderer->setViewMatrix(camera->getViewMatrix());
	Renderer->setModelMatrix(rotation * scaling);

	transparentObject.render(transparencyShader, ssaoHelper.isPreRenderPass());
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
	fpsArrayOffset = (fpsArrayOffset + 1) % fpsArray.size();
	fpsArray[fpsArrayOffset] = 1.0f/dt;

	if (perfMeasurementMode && !measurer->update(dt)) {
		// All modes were tested -> quit
		quit();
	}



	ImGuiIO &io = ImGui::GetIO();
	if (io.WantCaptureKeyboard) {
		// Ignore inputs below
		return;
	}


	for (int i = 0; i < NUM_OIT_MODES; i++) {
		if (Keyboard->keyPressed(SDLK_0+i)) {
			setRenderMode((RenderModeOIT)i);
		}
	}


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
