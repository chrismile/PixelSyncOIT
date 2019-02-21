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
#include "Utils/HairLoader.hpp"
#include "OIT/OIT_Dummy.hpp"
#include "OIT/OIT_KBuffer.hpp"
#include "OIT/OIT_LinkedList.hpp"
#include "OIT/OIT_MLAB.hpp"
#include "OIT/OIT_MLABBucket.hpp"
#include "OIT/OIT_HT.hpp"
#include "OIT/OIT_MBOIT.hpp"
#include "OIT/OIT_DepthComplexity.hpp"
#include "OIT/OIT_DepthPeeling.hpp"
#include "OIT/TilingMode.hpp"
#include "VoxelRaytracing/OIT_VoxelRaytracing.hpp"
#include "Tests/TestPixelSyncPerformance.hpp"
#include "MainApp.hpp"

void openglErrorCallback()
{
    std::cerr << "Application callback" << std::endl;
}

PixelSyncApp::PixelSyncApp() : camera(new Camera()), measurer(NULL), videoWriter(NULL),
                               cameraPath({/*ControlPoint(0.0f, 0.3f, 0.325f, 1.005f, 0.0f, 0.0f),
                                           ControlPoint(4.0f, 0.8f, 0.325f, 1.005f, -sgl::PI / 4.0f, 0.0f),*/

                                           ControlPoint(0, 0.3, 0.325, 1.005, -1.5708, 0),
                                           ControlPoint(3, 0.172219, 0.325, 1.21505, -1.15908, 0.0009368),
                                           ControlPoint(6, -0.229615, 0.350154, 1.00435, -0.425731, 0.116693),
                                           ControlPoint(9, -0.09407, 0.353779, 0.331819, 0.563857, 0.0243558),
                                           ControlPoint(12, 0.295731, 0.366529, -0.136542, 1.01983, -0.20646),
                                           ControlPoint(15, 1.13902, 0.444444, -0.136205, 2.46893, -0.320944),
                                           ControlPoint(18, 1.02484, 0.444444, 0.598137, 3.89793, -0.296935),
                                           ControlPoint(21, 0.850409, 0.470433, 0.976859, 4.02133, -0.127355),
                                           ControlPoint(24, 0.390787, 0.429582, 1.0748, 4.42395, -0.259301),
                                           ControlPoint(26, 0.3, 0.325, 1.005, -1.5708, 0)})
{
    plainShader = ShaderManager->getShaderProgram({"Mesh.Vertex.Plain", "Mesh.Fragment.Plain"});
    whiteSolidShader = ShaderManager->getShaderProgram({"WhiteSolid.Vertex", "WhiteSolid.Fragment"});

    EventManager::get()->addListener(RESOLUTION_CHANGED_EVENT, [this](EventPtr event){ this->resolutionChanged(event); });

    camera->setNearClipDistance(0.01f);
    camera->setFarClipDistance(100.0f);
    camera->setOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    fovy = atanf(1.0f / 2.0f) * 2.0f;
    camera->setFOVy(fovy);
    //camera->setPosition(glm::vec3(0.5f, 0.5f, 20.0f));
    camera->setPosition(glm::vec3(0.0f, -0.1f, 2.4f));

    bandingColor = Color(165, 220, 84, 120);
    if (perfMeasurementMode) {
        // Transparent background in measurement mode! This way, reference metrics can compare opacity values.
        //clearColor = Color(0, 0, 0, 0);
        clearColor = Color(255, 255, 255, 255);
    } else {
        clearColor = Color(255, 255, 255, 255);
    }
    clearColorSelection = ImColor(clearColor.getColorRGBA());
    transferFunctionWindow.setClearColor(clearColor);
    setNewTilingMode(2, 8);

    bool useVsync = AppSettings::get()->getSettings().getBoolValue("window-vSync");
    if (useVsync) {
        Timer->setFPSLimit(true, 60);
    } else {
        Timer->setFPSLimit(false, 60);
    }

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

    ShaderManager->addPreprocessorDefine("IMPORTANCE_CRITERION_INDEX", (int)importanceCriterionType);
    ShaderManager->addPreprocessorDefine("REFLECTION_MODEL", (int)reflectionModelType);

    shadowTechnique = boost::shared_ptr<ShadowTechnique>(new NoShadowMapping);
    updateAOMode();

    setRenderMode(mode, true);
    loadModel(MODEL_FILENAMES[usedModelIndex]);


    if (perfMeasurementMode) {
        sgl::FileUtils::get()->ensureDirectoryExists("images");
        measurer = new AutoPerfMeasurer(getAllTestModes(), "performance.csv",
                                        [this](const InternalState &newState) { this->setNewState(newState); });
        measurer->resolutionChanged(sceneFramebuffer);
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
    textureSettings.internalFormat = GL_RGBA8; // For i965 driver to accept image load/store
    textureSettings.pixelType = GL_UNSIGNED_BYTE;
    textureSettings.pixelFormat = GL_RGBA;
    sceneTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    sceneFramebuffer->bindTexture(sceneTexture);
    sceneDepthRBO = Renderer->createRBO(width, height, DEPTH24_STENCIL8);
    sceneFramebuffer->bindRenderbuffer(sceneDepthRBO, DEPTH_STENCIL_ATTACHMENT);


    camera->onResolutionChanged(event);
    camera->onResolutionChanged(event);
    oitRenderer->resolutionChanged(sceneFramebuffer, sceneTexture, sceneDepthRBO);
    if (currentAOTechnique == AO_TECHNIQUE_SSAO) {
        ssaoHelper->resolutionChanged();
    }
    if (perfMeasurementMode && measurer != NULL) {
        measurer->resolutionChanged(sceneFramebuffer);
    }
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



void PixelSyncApp::loadModel(const std::string &filename, bool resetCamera)
{
    // Pure filename without extension (to create compressed .binmesh filename)
    modelFilenamePure = FileUtils::get()->removeExtension(filename);

    if (oitRenderer->isTestingMode()) {
        return;
    }

    std::string modelFilenameOptimized = modelFilenamePure + ".binmesh";
    if (boost::ends_with(MODEL_DISPLAYNAMES[usedModelIndex], "(Lines)")) {
        // Special mode for line trajectories: Trajectories loaded as line set or as triangle mesh
        modelFilenameOptimized += "_lines";
        usesGeometryShader = true;
    } else {
        usesGeometryShader = false;
    }

    std::string modelFilenameObj = modelFilenamePure + ".obj";
    if (!FileUtils::get()->exists(modelFilenameOptimized)) {
        if (boost::starts_with(modelFilenamePure, "Data/Models")) {
            convertObjMeshToBinary(modelFilenameObj, modelFilenameOptimized);
        } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
            if (boost::ends_with(modelFilenameOptimized, "_tri")) {
                convertObjTrajectoryDataToBinaryLineMesh(modelFilenameObj, modelFilenameOptimized);
            } else {
                convertObjTrajectoryDataToBinaryTriangleMesh(modelFilenameObj, modelFilenameOptimized);
            }
        } else if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            modelFilenameObj = filename;
            convertHairDataToBinaryTriangleMesh(modelFilenameObj, modelFilenameOptimized);
        }
    }
    if (boost::starts_with(modelFilenamePure, "Data/Models")) {
        gatherShaderIDs = {"PseudoPhong.Vertex", "PseudoPhong.Fragment"};
    } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
        if (boost::ends_with(modelFilenameOptimized, "_lines")) {
            gatherShaderIDs = {"PseudoPhongVorticity.Vertex", "PseudoPhongVorticity.Geometry",
                               "PseudoPhongVorticity.Fragment"};
        } else {
            gatherShaderIDs = {"PseudoPhongVorticity.TriangleVertex", "PseudoPhongVorticity.Fragment"};
        }
    } else if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
        gatherShaderIDs = {"PseudoPhongHair.Vertex", "PseudoPhongHair.Fragment"};
    }

    updateShaderMode(SHADER_MODE_UPDATE_NEW_MODEL);

    if (mode != RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        transparentObject = parseMesh3D(modelFilenameOptimized, transparencyShader, shuffleGeometry);
        if (shaderMode == SHADER_MODE_VORTICITY) {
            recomputeHistogramForMesh();
        }
        boundingBox = transparentObject.boundingBox;

        if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            bool changed = false;
            bool hasColorArray = transparentObject.hasAttributeWithName("vertexColor");
            if (hasColorArray && !colorArrayMode) {
                sgl::ShaderManager->addPreprocessorDefine("COLOR_ARRAY", "");
                changed = true;
            } else if (!hasColorArray && colorArrayMode){
                sgl::ShaderManager->removePreprocessorDefine("COLOR_ARRAY");
                changed = true;
            }
            if (changed) {
                colorArrayMode = !colorArrayMode;
                sgl::ShaderManager->invalidateShaderCache();
                updateShaderMode(SHADER_MODE_UPDATE_NEW_MODEL);
                transparentObject.setNewShader(transparencyShader);
            }
        }
    } else {
        std::vector<float> lineAttributes;
        OIT_VoxelRaytracing *voxelRaytracer = (OIT_VoxelRaytracing*)oitRenderer.get();
        float maxVorticity = 0.0f;
        voxelRaytracer->loadModel(usedModelIndex, lineAttributes, maxVorticity);
        transferFunctionWindow.computeHistogram(lineAttributes, 0.0f, maxVorticity);
        transparentObject = MeshRenderer();
    }

    rotation = glm::mat4(1.0f);
    scaling = glm::mat4(1.0f);

    // Set position & banding mode dependent on the model
    if (resetCamera) {
        camera->setYaw(-sgl::PI / 2.0f);
        camera->setPitch(0.0f);
    }
    if (modelFilenamePure == "Data/Models/Ship_04") {
        transparencyShader->setUniform("bandedColorShading", 0);
        if (resetCamera) {
            camera->setPosition(glm::vec3(0.0f, 1.5f, 5.0f));
        }
    } else {
        if (shaderMode != SHADER_MODE_VORTICITY && !boost::starts_with(modelFilenamePure, "Data/Hair")) {
            transparencyShader->setUniform("bandedColorShading", 1);
        }

        if (resetCamera) {
            if (modelFilenamePure == "Data/Models/dragon") {
                camera->setPosition(glm::vec3(0.15f, 0.8f, 2.4f));
            } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories/lagranto")) {
                camera->setPosition(glm::vec3(0.6f, 0.0f, 8.8f));
            } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories/single_streamline")) {
                camera->setPosition(glm::vec3(0.72f, 0.215f, 0.2f));
            } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
                //camera->setPosition(glm::vec3(0.6f, 0.4f, 1.8f));
                camera->setPosition(glm::vec3(0.3f, 0.325f, 1.005f));
            } else if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
                //camera->setPosition(glm::vec3(0.6f, 0.4f, 1.8f));
                camera->setPosition(glm::vec3(0.3f, 0.325f, 1.005f));
            } else {
                camera->setPosition(glm::vec3(0.0f, -0.1f, 2.4f));
            }
        }
        if (modelFilenamePure == "Data/Models/dragon") {
            const float scalingFactor = 0.2f;
            scaling = matrixScaling(glm::vec3(scalingFactor));
        }
        if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            //transparencyShader->setUniform("bandedColorShading", 0);
            scaling = matrixScaling(glm::vec3(HAIR_MODEL_SCALING_FACTOR));
        }
    }


    boundingBox = boundingBox.transformed(rotation * scaling);
    updateAOMode();
    shadowTechnique->newModelLoaded(modelFilenamePure);
    shadowTechnique->setLightDirection(lightDirection, boundingBox);
    reRender = true;
}

void PixelSyncApp::recomputeHistogramForMesh()
{
    ShaderManager->addPreprocessorDefine("IMPORTANCE_CRITERION_INDEX", (int)importanceCriterionType);
    ImportanceCriterionAttribute importanceCriterionAttribute =
            transparentObject.importanceCriterionAttributes.at((int)importanceCriterionType);
    minCriterionValue = importanceCriterionAttribute.minAttribute;
    maxCriterionValue = importanceCriterionAttribute.maxAttribute;
    transferFunctionWindow.computeHistogram(importanceCriterionAttribute.attributes,
            minCriterionValue, maxCriterionValue);
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
    } else if (mode == RENDER_MODE_OIT_MLAB_BUCKET) {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_MLABBucket);
    } else if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_VoxelRaytracing(camera, clearColor));
    } else if (mode == RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE) {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new TestPixelSyncPerformance);
    } else {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_Dummy);
        Logfile::get()->writeError("PixelSyncApp::setRenderMode: Invalid mode.");
        mode = RENDER_MODE_OIT_DUMMY;
    }
    oitRenderer->setRenderSceneFunction([this]() { this->renderScene(); });

    updateShaderMode(SHADER_MODE_UPDATE_NEW_OIT_RENDERER);

    transparencyShader = oitRenderer->getGatherShader();

    if (oldMode == RENDER_MODE_VOXEL_RAYTRACING_LINES && mode != RENDER_MODE_VOXEL_RAYTRACING_LINES
            && !transparentObject.isLoaded()) {
        loadModel(MODEL_FILENAMES[usedModelIndex], false);
    }
    if (oldMode == RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE) {
        loadModel(MODEL_FILENAMES[usedModelIndex], true);
    }

    if (transparentObject.isLoaded() && mode != RENDER_MODE_VOXEL_RAYTRACING_LINES
            && !oitRenderer->isTestingMode()) {
        transparentObject.setNewShader(transparencyShader);
        if (shaderMode != SHADER_MODE_VORTICITY) {
            if (modelFilenamePure == "Data/Models/Ship_04") {
                transparencyShader->setUniform("bandedColorShading", 0);
            } else if (!boost::starts_with(modelFilenamePure, "Data/Hair")) {
                transparencyShader->setUniform("bandedColorShading", 1);
            }
        }
    }
    if (modelFilenamePure.length() > 0 && mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        std::vector<float> lineAttributes;
        OIT_VoxelRaytracing *voxelRaytracer = (OIT_VoxelRaytracing*)oitRenderer.get();
        float maxVorticity = 0.0f;
        voxelRaytracer->loadModel(usedModelIndex, lineAttributes, maxVorticity);
        transferFunctionWindow.computeHistogram(lineAttributes, 0.0f, maxVorticity);
    }
    if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        OIT_VoxelRaytracing *voxelRaytracer = (OIT_VoxelRaytracing*)oitRenderer.get();
        voxelRaytracer->setLineRadius(lineRadius);
        voxelRaytracer->setClearColor(clearColor);
        voxelRaytracer->setLightDirection(lightDirection);
        voxelRaytracer->setTransferFunctionTexture(transferFunctionWindow.getTransferFunctionMapTexture());
    }

    resolutionChanged(EventPtr());
    oldMode = mode;
}

void PixelSyncApp::updateShaderMode(ShaderModeUpdate modeUpdate)
{
    if (oitRenderer->isTestingMode()) {
        return;
    }

    if (gatherShaderIDs.size() != 0) {
        oitRenderer->setGatherShaderList(gatherShaderIDs);
        shadowTechnique->setGatherShaderList(gatherShaderIDs);
        transparencyShader = oitRenderer->getGatherShader();
    }
    // TODO: SHADER_MODE_UPDATE_SSAO_CHANGE
    if (boost::starts_with(modelFilenamePure, "Data/Trajectories/")) {
        if (shaderMode != SHADER_MODE_VORTICITY || modeUpdate == SHADER_MODE_UPDATE_NEW_OIT_RENDERER
                || modeUpdate == SHADER_MODE_UPDATE_SSAO_CHANGE) {
            shaderMode = SHADER_MODE_VORTICITY;
        }
    } else {
        if (shaderMode == SHADER_MODE_VORTICITY || modeUpdate == SHADER_MODE_UPDATE_SSAO_CHANGE) {
            shaderMode = SHADER_MODE_PSEUDO_PHONG;
        }
    }
}

void PixelSyncApp::setNewState(const InternalState &newState)
{
    // 1. Test whether fragment shader invocation interlock (Pixel Sync) or atomic operations shall be disabled
    if (newState.testNoInvocationInterlock) {
        ShaderManager->addPreprocessorDefine("TEST_NO_INVOCATION_INTERLOCK", "");
    } else {
        ShaderManager->removePreprocessorDefine("TEST_NO_INVOCATION_INTERLOCK");
    }
    if (newState.testNoAtomicOperations) {
        ShaderManager->addPreprocessorDefine("TEST_NO_ATOMIC_OPERATIONS", "");
    } else {
        ShaderManager->removePreprocessorDefine("TEST_NO_ATOMIC_OPERATIONS");
    }
    if (newState.testPixelSyncOrdered) {
        ShaderManager->addPreprocessorDefine("PIXEL_SYNC_ORDERED", "");
    } else {
        ShaderManager->removePreprocessorDefine("PIXEL_SYNC_ORDERED");
    }

    // 2. Handle global state changes like SSAO, tiling mode
    setNewTilingMode(newState.tilingWidth, newState.tilingHeight, newState.useMortonCodeForTiling);
    if (currentAOTechnique != newState.aoTechnique) {
        currentAOTechnique = newState.aoTechnique;
        updateAOMode();
        ShaderManager->invalidateShaderCache();
        updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
    }

    // 3. Load right model file
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
    shuffleGeometry = newState.testShuffleGeometry;
    loadModel(modelFilename);

    // 4. Set OIT algorithm
    setRenderMode(newState.oitAlgorithm, true);

    // 5. Pass state change to OIT mode to handle internally necessary state changes.
    if (firstState || newState != lastState) {
        oitRenderer->setNewState(newState);
    }

    lastState = newState;
    firstState = false;
}

void PixelSyncApp::updateAOMode()
{
    // First delete/remove old SSAO data
    if (ssaoHelper != NULL) {
        delete ssaoHelper;
        ssaoHelper = NULL;
    }
    if (voxelAOHelper != NULL) {
        delete voxelAOHelper;
        voxelAOHelper = NULL;
    }

    // Now, set data for new technique
    if (currentAOTechnique == AO_TECHNIQUE_NONE) {
        ShaderManager->removePreprocessorDefine("USE_SSAO");
        ShaderManager->removePreprocessorDefine("VOXEL_SSAO");
        ssaoHelper = new SSAOHelper();
    }
    if (currentAOTechnique == AO_TECHNIQUE_SSAO) {
        ShaderManager->removePreprocessorDefine("VOXEL_SSAO");
        ShaderManager->addPreprocessorDefine("USE_SSAO", "");
        ssaoHelper = new SSAOHelper();
    }
    if (currentAOTechnique == AO_TECHNIQUE_VOXEL_AO) {
        ShaderManager->removePreprocessorDefine("USE_SSAO");
        ShaderManager->addPreprocessorDefine("VOXEL_SSAO", "");
        voxelAOHelper = new VoxelAOHelper();
        voxelAOHelper->loadAOFactorsFromVoxelFile(modelFilenamePure);
    }
}

void PixelSyncApp::updateShadowMode()
{
    if (currentShadowTechnique == NO_SHADOW_MAPPING) {
        shadowTechnique = boost::shared_ptr<ShadowTechnique>(new NoShadowMapping);
    } else if (currentShadowTechnique == SHADOW_MAPPING) {
        shadowTechnique = boost::shared_ptr<ShadowTechnique>(new ShadowMapping);
    } else if (currentShadowTechnique == MOMENT_SHADOW_MAPPING) {
        shadowTechnique = boost::shared_ptr<ShadowTechnique>(new MomentShadowMapping);
    }

    shadowTechnique->setLightDirection(lightDirection, boundingBox);
    shadowTechnique->setShaderDefines();
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

    // Delete SSAO data
    if (ssaoHelper != NULL) {
        delete ssaoHelper;
        ssaoHelper = NULL;
    }
    if (voxelAOHelper != NULL) {
        delete voxelAOHelper;
        voxelAOHelper = NULL;
    }

    if (perfMeasurementMode) {
        delete measurer;
        measurer = NULL;
    }

    if (videoWriter != NULL) {
        delete videoWriter;
    }
}

#include <Graphics/OpenGL/RendererGL.hpp>
void PixelSyncApp::render()
{
    if (videoWriter == NULL && recording) {
        videoWriter = new VideoWriter("video.mp4", 60);
    }


    reRender = reRender || oitRenderer->needsReRender() || oitRenderer->isTestingMode();

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
    sgl::RendererGL *rgl = (sgl::RendererGL*)Renderer;

    renderGUI();

    // Video recording enabled?
    if (recording) {
        Renderer->bindFBO(sceneFramebuffer);
        videoWriter->pushWindowFrame();
        Renderer->unbindFBO();
    }
}


void PixelSyncApp::renderOIT()
{
    bool wireframe = false;

    if (mode == RENDER_MODE_OIT_MBOIT) {
        AABB3 screenSpaceBoundingBox = boundingBox.transformed(camera->getViewMatrix());
        static_cast<OIT_MBOIT*>(oitRenderer.get())->setScreenSpaceBoundingBox(screenSpaceBoundingBox, camera);
    }
    if (mode == RENDER_MODE_OIT_MLAB_BUCKET) {
        AABB3 screenSpaceBoundingBox = boundingBox.transformed(camera->getViewMatrix());
        static_cast<OIT_MLABBucket*>(oitRenderer.get())->setScreenSpaceBoundingBox(screenSpaceBoundingBox, camera);
    }
    if (currentShadowTechnique == MOMENT_SHADOW_MAPPING) {
        static_cast<MomentShadowMapping*>(shadowTechnique.get())->setSceneBoundingBox(boundingBox);
    }

    if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        if (currentAOTechnique == AO_TECHNIQUE_VOXEL_AO) {
            voxelAOHelper->setUniformValues(oitRenderer->getGatherShader());
        }

#ifdef PROFILING_MODE
        oitRenderer->renderToScreen();
#else
        if (perfMeasurementMode) {
            measurer->startMeasure();
        }

        oitRenderer->renderToScreen();

        if (perfMeasurementMode) {
            measurer->endMeasure();
        }
#endif
        return;
    }
    //Renderer->setBlendMode(BLEND_ALPHA);

    if (currentAOTechnique == AO_TECHNIQUE_SSAO) {
        ssaoHelper->preRender([this]() { this->renderScene(); });
    }

    if (currentShadowTechnique != NO_SHADOW_MAPPING) {
        shadowTechnique->createShadowMapPass([this]() { this->renderScene(); });
    }

    Renderer->bindFBO(sceneFramebuffer);
    /*if (perfMeasurementMode) {
        // Transparent background in measurement mode! This way, reference metrics can compare opacity values.
        Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, Color(0,0,0,0));
    } else {
        Renderer->clearFramebuffer(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, clearColor);
    }*/
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
    oitRenderer->renderScene();
    oitRenderer->gatherEnd();

    oitRenderer->renderToScreen();

    if (perfMeasurementMode) {
        measurer->endMeasure();
    }
#endif

    // Render light direction sphere TODO
    /*if (true) {
        auto solidShader = sgl::ShaderManager->getShaderProgram({"Mesh.Vertex.Plain", "Mesh.Fragment.Plain"});
        solidShader->setUniform("color", sgl::Color(255, 255, 0));
        auto lightPlaneRenderData = sgl::ShaderManager->createShaderAttributes(solidShader);

        Renderer->setViewMatrix(camera->getViewMatrix());
        Renderer->setProjectionMatrix(camera->getProjectionMatrix());

        std::vector<glm::vec3> lightPlane{
                glm::vec3(0.1,0.1,0), glm::vec3(-0.1,-0.1,0), glm::vec3(0.1,-0.1,0),
                glm::vec3(-0.1,-0.1,0), glm::vec3(0.1,0.1,0), glm::vec3(-0.1,0.1,0)};
        sgl::GeometryBufferPtr lightPlaneBuffer = sgl::Renderer->createGeometryBuffer(
                sizeof(glm::vec3)*lightPlane.size(), (void*)&lightPlane.front());
        lightPlaneRenderData->addGeometryBuffer(lightPlaneBuffer, "vertexPosition", sgl::ATTRIB_FLOAT, 3);

        glEnable(GL_DEPTH_TEST);
        Renderer->render(lightPlaneRenderData);
        glDisable(GL_DEPTH_TEST);
    }*/

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
        if (ImGui::Begin("Settings", &showSettingsWindow)) {
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

            oitRenderer->renderGUI();
        }
        ImGui::End();
    }

    if (transferFunctionWindow.renderGUI()) {
        reRender = true;
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

    if (ImGui::ColorEdit3("Clear Color", (float*)&clearColorSelection, 0)) {
        clearColor = colorFromFloat(clearColorSelection.x, clearColorSelection.y, clearColorSelection.z,
                                    clearColorSelection.w);
        if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
            static_cast<OIT_VoxelRaytracing*>(oitRenderer.get())->setClearColor(clearColor);
        }
        transferFunctionWindow.setClearColor(clearColor);
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
        if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
            static_cast<OIT_VoxelRaytracing*>(oitRenderer.get())->setLightDirection(lightDirection);
        }
        if (currentShadowTechnique != NO_SHADOW_MAPPING) {
            shadowTechnique->setLightDirection(lightDirection, boundingBox);
        }
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
    ImGui::Checkbox("UI on Screenshot", &uiOnScreenshot);

    if (shaderMode == SHADER_MODE_VORTICITY) {
        ImGui::SameLine();
        if (ImGui::Checkbox("Transparency", &transparencyMapping)) {
            reRender = true;
        }
        ImGui::Checkbox("Show transfer function window", &transferFunctionWindow.getShowTransferFunctionWindow());

        if (usesGeometryShader && ImGui::SliderFloat("Line radius", &lineRadius, 0.0001f, 0.01f, "%.4f")) {
            if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
                static_cast<OIT_VoxelRaytracing*>(oitRenderer.get())->setLineRadius(lineRadius);
            }
            reRender = true;
        }

        // Switch importance criterion
        if (!usesGeometryShader && mode != RENDER_MODE_VOXEL_RAYTRACING_LINES
                && ImGui::Combo("Importance Criterion", (int*)&importanceCriterionType, IMPORTANCE_CRITERION_DISPLAYNAMES,
                        IM_ARRAYSIZE(IMPORTANCE_CRITERION_DISPLAYNAMES))) {
            recomputeHistogramForMesh();
            ShaderManager->invalidateShaderCache();
            updateAOMode();
            updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
            reRender = true;
        }

    }

    if (ImGui::Combo("AO Mode", (int*)&currentAOTechnique, AO_TECHNIQUE_DISPLAYNAMES,
                     IM_ARRAYSIZE(AO_TECHNIQUE_DISPLAYNAMES))) {
        ShaderManager->invalidateShaderCache();
        updateAOMode();
        updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
        reRender = true;
    }

    if (ImGui::SliderFloat("AO Factor", &aoFactor, 0.0f, 1.0f)) {
        reRender = true;
    }

    if (ImGui::Combo("Shadow Mode", (int*)&currentShadowTechnique, SHADOW_MAPPING_TECHNIQUE_DISPLAYNAMES,
                     IM_ARRAYSIZE(SHADOW_MAPPING_TECHNIQUE_DISPLAYNAMES))) {
        ShaderManager->invalidateShaderCache();
        updateShadowMode();
        updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
        reRender = true;
    }

    if (ImGui::SliderFloat("Shadow Factor", &shadowFactor, 0.0f, 1.0f)) {
        reRender = true;
    }

    if (ImGui::Combo("Reflection Model", (int*)&reflectionModelType, REFLECTION_MODEL_DISPLAY_NAMES,
                     IM_ARRAYSIZE(REFLECTION_MODEL_DISPLAY_NAMES))) {
        ShaderManager->addPreprocessorDefine("REFLECTION_MODEL", (int)reflectionModelType);
        ShaderManager->invalidateShaderCache();
        updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
        reRender = true;
    }

    if (shadowTechnique->renderGUI()) {
        reRender = true;

        // Needs new transparency shader because of changed global settings?
        if (shadowTechnique->getNeedsNewTransparencyShader()) {
            updateShaderMode(SHADER_MODE_UPDATE_SSAO_CHANGE);
        }
    }

    ImGui::SliderFloat("Move speed", &MOVE_SPEED, 0.1f, 1.0f);

    if (ImGui::Checkbox("Shuffle Geometry", &shuffleGeometry)) {
        loadModel(MODEL_FILENAMES[usedModelIndex], false);
        reRender = true;
    }
    if (ImGui::Button("Shuffle")) {
        shuffleGeometry = true;
        loadModel(MODEL_FILENAMES[usedModelIndex], false);
        reRender = true;
    }
}

sgl::ShaderProgramPtr PixelSyncApp::setUniformValues()
{
    ShaderProgramPtr transparencyShader;
    bool isHairDataset = boost::starts_with(modelFilenamePure, "Data/Hair");

    if ((currentAOTechnique != AO_TECHNIQUE_SSAO || !ssaoHelper->isPreRenderPass())
            && (currentShadowTechnique == NO_SHADOW_MAPPING || !shadowTechnique->isShadowMapCreatePass())) {
        transparencyShader = oitRenderer->getGatherShader();
        if (shaderMode == SHADER_MODE_VORTICITY) {
            transparencyShader->setUniform("minCriterionValue", minCriterionValue);
            transparencyShader->setUniform("maxCriterionValue", maxCriterionValue);
            if (transparencyShader->hasUniform("radius")) {
                transparencyShader->setUniform("radius", lineRadius);
            }
            transparencyShader->setUniform("transparencyMapping", transparencyMapping);
            transparencyShader->setUniform("transferFunctionTexture",
                    transferFunctionWindow.getTransferFunctionMapTexture(), 5);
            //transparencyShader->setUniformBuffer(2, "TransferFunctionBlock",
            //      transferFunctionWindow.getTransferFunctionMapUBO());
            //std::cout << "Max Vorticity: " << *maxVorticity << std::endl;
            //transparencyShader->setUniform("cameraPosition", -camera->getPosition());
        }

        if (shaderMode != SHADER_MODE_VORTICITY) {
            // Hack for supporting multiple passes...
            if (modelFilenamePure == "Data/Models/Ship_04") {
                transparencyShader->setUniform("bandedColorShading", 0);
            } else if (!isHairDataset) {
                transparencyShader->setUniform("bandedColorShading", 1);
            }
        }

        if (!isHairDataset) {
            transparencyShader->setUniform("colorGlobal", bandingColor);
        }
        if (transparencyShader->hasUniform("lightDirection")) {
            transparencyShader->setUniform("lightDirection", lightDirection);
        }
    }

    if (currentShadowTechnique != NO_SHADOW_MAPPING) {
        if (shadowTechnique->isShadowMapCreatePass()) {
            transparencyShader = shadowTechnique->getShadowMapCreationShader();
            shadowTechnique->setUniformValuesCreateShadowMap();
            if (shaderMode == SHADER_MODE_VORTICITY) {
                transparencyShader->setUniform("minCriterionValue", minCriterionValue);
                transparencyShader->setUniform("maxCriterionValue", maxCriterionValue);
                if (transparencyShader->hasUniform("radius")) {
                    transparencyShader->setUniform("radius", lineRadius);
                }
                transparencyShader->setUniform("transparencyMapping", transparencyMapping);
                transparencyShader->setUniform("transferFunctionTexture",
                                               transferFunctionWindow.getTransferFunctionMapTexture(), 5);
            }
        }
        if (!shadowTechnique->isShadowMapCreatePass()) {
            transparencyShader = oitRenderer->getGatherShader();
            shadowTechnique->setUniformValuesRenderScene(transparencyShader);
        }

    }

    if (!shadowTechnique->isShadowMapCreatePass()) {
        if (currentAOTechnique == AO_TECHNIQUE_SSAO
                && (!transparencyShader || transparencyShader->hasUniform("ssaoTexture"))) {
            if (ssaoHelper->isPreRenderPass()) {
                transparencyShader = ssaoHelper->getGeometryPassShader();
            } else {
                TexturePtr ssaoTexture = ssaoHelper->getSSAOTexture();
                transparencyShader->setUniform("ssaoTexture", ssaoTexture, 4);
            }
        } else if (currentAOTechnique == AO_TECHNIQUE_VOXEL_AO && transparencyShader->hasUniform("aoTexture")) {
            voxelAOHelper->setUniformValues(transparencyShader);
        }
    }


    if (currentAOTechnique != AO_TECHNIQUE_NONE
            && (currentAOTechnique != AO_TECHNIQUE_SSAO || !ssaoHelper->isPreRenderPass())
            && reflectionModelType != LOCAL_SHADOW_MAP_OCCLUSION) {
        transparencyShader->setUniform("aoFactorGlobal", aoFactor);
    }
    if (currentShadowTechnique != NO_SHADOW_MAPPING && !shadowTechnique->isShadowMapCreatePass()
            && reflectionModelType != AMBIENT_OCCLUSION_FACTOR) {
        transparencyShader->setUniform("shadowFactorGlobal", shadowFactor);
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

    if (!shadowTechnique->isShadowMapCreatePass()) {
        Renderer->setProjectionMatrix(camera->getProjectionMatrix());
        Renderer->setViewMatrix(camera->getViewMatrix());
    }
    Renderer->setModelMatrix(rotation * scaling);

    bool isGBufferPass = currentAOTechnique == AO_TECHNIQUE_SSAO && ssaoHelper->isPreRenderPass();
    transparentObject.render(transparencyShader, isGBufferPass);
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

    if (recording || testCameraFlight) {
        // Already recorded full cycle?
        if (recordingTime > FULL_CIRCLE_TIME) {
            quit();
        }

        // Otherwise, update camera position
        float updateRate = 1.0f/FRAME_RATE;
        recordingTime += updateRate;

        cameraPath.update(updateRate);
        camera->overwriteViewMatrix(cameraPath.getViewMatrix());
        /*float circleAngle = recordingTime / FULL_CIRCLE_TIME * sgl::TWO_PI;
        glm::vec3 cameraPosition = cameraLookAtCenter + rotationRadius*glm::vec3(cosf(circleAngle), 0.0f, sinf(circleAngle));
        camera->setPosition(cameraPosition);
        //glm::lookAt()
        camera->setYaw(circleAngle + sgl::PI);*/

        reRender = true;
    }
    if (testOutputPos && Keyboard->keyPressed(SDLK_p)) {
        // ControlPoint(0.0f, 0.3f, 0.325f, 1.005f, 0.0f, 0.0f),
        std::cout << "ControlPoint(" << outputTime << ", " << camera->getPosition().x << ", " << camera->getPosition().y
                << ", " << camera->getPosition().z << ", " << camera->getYaw() << ", " << camera->getPitch()
                << ")," << std::endl;
        outputTime += 1.0f;
    }



    transferFunctionWindow.update(dt);

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


    glm::mat4 rotationMatrix = camera->getRotationMatrix();//glm::mat4(camera->getOrientation());
    glm::mat4 invRotationMatrix = glm::inverse(rotationMatrix);
    if (Keyboard->isKeyDown(SDLK_PAGEDOWN)) {
        camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, -dt*MOVE_SPEED, 0.0f)));
        reRender = true;
    }
    if (Keyboard->isKeyDown(SDLK_PAGEUP)) {
        camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, dt*MOVE_SPEED, 0.0f)));
        reRender = true;
    }
    if (Keyboard->isKeyDown(SDLK_DOWN) || Keyboard->isKeyDown(SDLK_s)) {
        camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, 0.0f, dt*MOVE_SPEED)));
        reRender = true;
    }
    if (Keyboard->isKeyDown(SDLK_UP) || Keyboard->isKeyDown(SDLK_w)) {
        camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, 0.0f, -dt*MOVE_SPEED)));
        reRender = true;
    }
    if (Keyboard->isKeyDown(SDLK_LEFT) || Keyboard->isKeyDown(SDLK_a)) {
        camera->translate(transformPoint(invRotationMatrix, glm::vec3(-dt*MOVE_SPEED, 0.0f, 0.0f)));
        reRender = true;
    }
    if (Keyboard->isKeyDown(SDLK_RIGHT) || Keyboard->isKeyDown(SDLK_d)) {
        camera->translate(transformPoint(invRotationMatrix, glm::vec3(dt*MOVE_SPEED, 0.0f, 0.0f)));
        reRender = true;
    }

    if (io.WantCaptureMouse) {
        // Ignore inputs below
        return;
    }

    // Zoom in/out
    if (Mouse->getScrollWheel() > 0.1 || Mouse->getScrollWheel() < -0.1) {
        fovy -= Mouse->getScrollWheel()*dt*2.0f;
        fovy = glm::clamp(fovy, glm::radians(1.0f), glm::radians(150.0f));
        camera->setFOVy(fovy);
        reRender = true;
    }


    // Mouse rotation
    if (Mouse->isButtonDown(1) && Mouse->mouseMoved()) {
        sgl::Point2 pixelMovement = Mouse->mouseMovement();
        float yaw = dt*MOUSE_ROT_SPEED*pixelMovement.x;
        float pitch = -dt*MOUSE_ROT_SPEED*pixelMovement.y;

        glm::quat rotYaw = glm::quat(glm::vec3(0.0f, yaw, 0.0f));
        glm::quat rotPitch = glm::quat(pitch*glm::vec3(rotationMatrix[0][0], rotationMatrix[1][0], rotationMatrix[2][0]));
        //camera->rotate(rotYaw*rotPitch);
        camera->rotateYaw(yaw);
        camera->rotatePitch(pitch);
        reRender = true;
    }
}
