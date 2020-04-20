/*
 * MainApp.cpp
 *
 *  Created on: 22.04.2017
 *      Author: Christoph Neuhauser
 */

#define GLM_ENABLE_EXPERIMENTAL
#include <climits>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <thread>

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
#include <Graphics/OpenGL/SystemGL.hpp>
#include <ImGui/ImGuiWrapper.hpp>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_custom.h>
#include <ImGui/imgui_stdlib.h>

#include "Utils/MeshSerializer.hpp"
#include "Utils/OBJLoader.hpp"
#include "Utils/BinaryObjLoader.hpp"
#include "Utils/PointRendering/PointFileLoader.hpp"
#include "Utils/TrajectoryLoader.hpp"
#include "Utils/HairLoader.hpp"
#include "OIT/BufferSizeWatch.hpp"
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
#ifdef USE_RAYTRACING
#include "Raytracing/OIT_RayTracing.hpp"
#endif

#include "MainApp.hpp"

void openglErrorCallback()
{
    std::cerr << "Application callback" << std::endl;
}

CameraSetting::CameraSetting(float time, float tx, float ty, float tz, float yaw_, float pitch_)
{
    position = glm::vec3(tx, ty, tz);
    yaw = yaw_;
    pitch = pitch_;
}

PixelSyncApp::PixelSyncApp() : camera(new Camera()), measurer(NULL), videoWriter(NULL)
{
    // https://www.khronos.org/registry/OpenGL/extensions/NVX/NVX_gpu_memory_info.txt
    GLint freeMemKilobytes = 0;
    if (perfMeasurementMode && sgl::SystemGL::get()->isGLExtensionAvailable("GL_NVX_gpu_memory_info")) {
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &freeMemKilobytes);
    }

    if (!SystemGL::get()->isGLExtensionAvailable("GL_ARB_fragment_shader_interlock")) {
        Logfile::get()->writeInfo("GL_ARB_fragment_shader_interlock unsupported. Switching to per-pixel linked lists.");
        mode = RENDER_MODE_OIT_LINKED_LIST;
    }

    sgl::FileUtils::get()->ensureDirectoryExists("Data/CameraPaths/");
    sgl::FileUtils::get()->ensureDirectoryExists(saveDirectoryScreenshots);

    for (int i = 0; i < NUM_MODELS; i++) {
        if (startupModelName == MODEL_DISPLAYNAMES[i]) {
            usedModelIndex = i;
            break;
        }
    }

    if (recording || perfMeasurementMode) {
        testCameraFlight = true;
        showSettingsWindow = false;
        transferFunctionWindow.setShow(false);
    }

    cameraPath.fromControlPoints({
        ControlPoint(0, 0.3, 0.325, 1.005, -1.5708, 0),
        ControlPoint(3, 0.172219, 0.325, 1.21505, -1.15908, 0.0009368),
        ControlPoint(6, -0.229615, 0.350154, 1.00435, -0.425731, 0.116693),
        ControlPoint(9, -0.09407, 0.353779, 0.331819, 0.563857, 0.0243558),
        ControlPoint(12, 0.295731, 0.366529, -0.136542, 1.01983, -0.20646),
        ControlPoint(15, 1.13902, 0.444444, -0.136205, 2.46893, -0.320944),
        ControlPoint(18, 1.02484, 0.444444, 0.598137, 3.89793, -0.296935),
        ControlPoint(21, 0.850409, 0.470433, 0.976859, 4.02133, -0.127355),
        ControlPoint(24, 0.390787, 0.429582, 1.0748, 4.42395, -0.259301),
        ControlPoint(26, 0.3, 0.325, 1.005, -1.5708, 0)});

    gammaCorrectionShader = ShaderManager->getShaderProgram({"GammaCorrection.Vertex", "GammaCorrection.Fragment"});

    EventManager::get()->addListener(RESOLUTION_CHANGED_EVENT,
            [this](EventPtr event){ this->resolutionChanged(event); });

    camera->setNearClipDistance(0.01f);
    camera->setFarClipDistance(100.0f);
    camera->setOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    fovy = atanf(1.0f / 2.0f) * 2.0f; // 90.0f / 180.0f * sgl::PI;//
    camera->setFOVy(fovy);
    //camera->setPosition(glm::vec3(0.5f, 0.5f, 20.0f));
    camera->setPosition(glm::vec3(0.0f, -0.1f, 2.4f));

    bandingColor = Color(165, 220, 84, 120);
    clearColor = Color(255, 255, 255, 255);
    clearColorSelection = ImColor(clearColor.getColorRGBA());
    transferFunctionWindow.setClearColor(clearColor);
    transferFunctionWindow.setUseLinearRGB(useLinearRGB);

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

    ShaderManager->addPreprocessorDefine("REFLECTION_MODEL", (int)reflectionModelType);

    shadowTechnique = boost::shared_ptr<ShadowTechnique>(new NoShadowMapping);
    updateAOMode();

    setRenderMode(mode, true);
    loadModel(MODEL_FILENAMES[usedModelIndex]);

    if ((testCameraFlight || recording) && recordingUseGlobalIlumination) {
        currentShadowTechnique = MOMENT_SHADOW_MAPPING;
        currentAOTechnique = AO_TECHNIQUE_VOXEL_AO;
        setHighResMomentShadowMapping();
        updateShadowMode();
        updateAOMode();
        ShaderManager->invalidateShaderCache();
        updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
    }



    if (perfMeasurementMode) {
        sgl::FileUtils::get()->ensureDirectoryExists("images");
        measurer = new AutoPerfMeasurer(getTestModesPaper(), "performance.csv", "depth_complexity.csv",
                                        [this](const InternalState &newState) { this->setNewState(newState); }, timeCoherence);
        measurer->setInitialFreeMemKilobytes(freeMemKilobytes);
        measurer->resolutionChanged(sceneFramebuffer);

        if (mode == RENDER_MODE_OIT_DEPTH_COMPLEXITY) {
            OIT_DepthComplexity *depthComplexityOIT = (OIT_DepthComplexity*)oitRenderer.get();

            if (depthComplexityOIT)
            {
                depthComplexityOIT->setPerfMeasurer(measurer);
            }
        }


        continuousRendering = true; // Always use continuous rendering in performance measurement mode
    } else {
        measurer = NULL;
    }

    recordingTimeStampStart = Timer->getTicksMicroseconds();
    usesNewState = true;
    frameNum = 0;
    recordingTime = 0.0f;
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
    if (useLinearRGB) {
        textureSettings.internalFormat = GL_RGBA16;
    } else {
        textureSettings.internalFormat = GL_RGBA8; // GL_RGBA8 For i965 driver to accept image load/store (legacy)
    }
    textureSettings.pixelType = GL_UNSIGNED_BYTE;
    textureSettings.pixelFormat = GL_RGB;
    sceneTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    sceneFramebuffer->bindTexture(sceneTexture);
    sceneDepthRBO = Renderer->createRBO(width, height, sgl::RBO_DEPTH24_STENCIL8);
    sceneFramebuffer->bindRenderbuffer(sceneDepthRBO, DEPTH_STENCIL_ATTACHMENT);

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

void PixelSyncApp::updateColorSpaceMode()
{
    Window *window = AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();
    glViewport(0, 0, width, height);

    // Buffers for off-screen rendering
    sceneFramebuffer = Renderer->createFBO();
    TextureSettings textureSettings;
    if (useLinearRGB) {
        textureSettings.internalFormat = GL_RGBA16;
    } else {
        textureSettings.internalFormat = GL_RGBA8; // GL_RGBA8 For i965 driver to accept image load/store (legacy)
    }
    textureSettings.pixelType = GL_UNSIGNED_BYTE;
    textureSettings.pixelFormat = GL_RGB;
    sceneTexture = TextureManager->createEmptyTexture(width, height, textureSettings);
    sceneFramebuffer->bindTexture(sceneTexture);
    sceneDepthRBO = Renderer->createRBO(width, height, sgl::RBO_DEPTH24_STENCIL8);
    sceneFramebuffer->bindRenderbuffer(sceneDepthRBO, DEPTH_STENCIL_ATTACHMENT);

    transferFunctionWindow.setUseLinearRGB(useLinearRGB);
}

void PixelSyncApp::saveScreenshot(const std::string &filename)
{
    if (!printNow && !uiOnScreenshot) {
        // Don't print at time sgl wants, as in this case we would need to include the GUI
        return;
    }
    std::string imageFilename = saveDirectoryScreenshots + saveFilenameScreenshots + "_"
            + std::to_string(numScreenshots++) + "_mode" + std::to_string(mode) + ".png";

    if (uiOnScreenshot) {
        AppLogic::saveScreenshot(imageFilename);
    } else {
        Window *window = AppSettings::get()->getMainWindow();
        int width = window->getWidth();
        int height = window->getHeight();

        BitmapPtr bitmap(new Bitmap(width, height, 32));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->getPixels());
        bitmap->savePNG(imageFilename.c_str(), true);
    }
}

void PixelSyncApp::saveScreenshotOnKey(const std::string &filename)
{
    if (uiOnScreenshot) {
        AppLogic::saveScreenshot(filename);
    } else {
        Window *window = AppSettings::get()->getMainWindow();
        int width = window->getWidth();
        int height = window->getHeight();

        BitmapPtr bitmap(new Bitmap(width, height, 32));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->getPixels());
        bitmap->savePNG(filename.c_str(), true);
    }
}

const uint32_t CAMERA_PATH_FORMAT_VERSION = 1u;

void PixelSyncApp::saveCameraPosition()
{
    auto now = std::chrono::system_clock::now();
    std::time_t end_time = std::chrono::system_clock::to_time_t(now);

    std::string modelName = modelFilenamePure;
    modelName.erase(std::remove(modelName.begin(), modelName.end(), ' '), modelName.end());

    std::string filename = std::string() + modelName + "_" + std::to_string(end_time);

    saveCameraPositionToFile(filename);
}


void PixelSyncApp::saveCameraPositionToFile(const std::string& filename)
{
    std::string fileBinPath = filename + ".binpath";
    std::string fileCamera = filename + ".camera";

    std::ofstream file(fileBinPath.c_str(), std::ofstream::binary);
    std::ofstream fileC(fileCamera.c_str());

    sgl::BinaryWriteStream stream;
    stream.write((uint32_t)CAMERA_PATH_FORMAT_VERSION);
    std::vector<CameraSetting> pointCamera;
    std::vector<ControlPoint> pointPath;

    auto cameraPos = camera->getPosition();
    float pitch = camera->getPitch();
    float yaw = camera->getYaw();


    glm::quat orientation = camera->getOrientation();


    pointCamera.emplace_back(CameraSetting(0, cameraPos.x, cameraPos.y, cameraPos.z, yaw, pitch));
    pointPath.emplace_back(ControlPoint(0, cameraPos.x, cameraPos.y, cameraPos.z, yaw, pitch));


    Logfile::get()->writeInfo(std::string() + "Saved camera position to file \"" + filename + "\".");

    stream.writeArray(pointPath);
    file.write((const char*)stream.getBuffer(), stream.getSize());
    file.close();

//    sgl::BinaryWriteStream streamC;
//    streamC.writeArray(pointCamera);

    fileC << cameraPos.x << " " << cameraPos.y << " " << cameraPos.z << " " << yaw << " " << pitch;
    fileC.close();
}


void PixelSyncApp::loadCameraPositionFromFile(const std::string& filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in CameraPath::fromBinaryFile: File \""
                                        + filename + "\" not found.");
        return;
    }

//    file.seekg(0, file.end);
//    size_t size = file.tellg();
//    file.seekg(0);
//    char *buffer = new char[size];
//    file.read(buffer, size);
//    file.close();

//    sgl::BinaryReadStream stream(buffer, size);
//    uint32_t version;
//    stream.read(version);
//    if (version != CAMERA_PATH_FORMAT_VERSION) {
//        sgl::Logfile::get()->writeError(std::string() + "Error in CameraPath::fromBinaryFile: "
//                                        + "Invalid version in file \"" + filename + "\".");
//
//        return;
//    }

//    std::vector<CameraSetting> point;
//    stream.readArray(point);

//    CameraSetting p0 = point[0];

//    glm::vec3 eulerAngles = glm::eulerAngles(p0.orientation);

    float posX, posY, posZ, yaw, pitch;

    file >> posX;
    file >> posY;
    file >> posZ;
    file >> yaw;
    file >> pitch;

    float fovy = camera->getFOVy();
    float aspect =  camera->getAspectRatio();
    auto viewport = camera->getViewport();

//    float yaw = yaw;
//    float pitch = pitch;
//
//    auto cam = camera.get();

//    rotation = glm::mat4(1.0f);
//    scaling = glm::mat4(1.0f);
//    boundingBox = transparentObject.boundingBox;
//    boundingBox = boundingBox.transformed(rotation * scaling);

    camera = boost::shared_ptr<Camera>(new Camera());
    camera->setViewport(viewport);
    camera->setFOVy(fovy);
    camera->setNearClipDistance(0.01f);
    camera->setFarClipDistance(100.0f);
//    camera->setOrientation(p0.orientation);
    camera->setYaw(yaw);
    camera->setPitch(pitch);

    //camera->setPosition(glm::vec3(0.5f, 0.5f, 20.0f));
    camera->setPosition(glm::vec3(posX, posY, posZ));
//
    resolutionChanged(EventPtr());



//    camera->setYaw(-eulerAngles.y);
//    camera->setPitch(-eulerAngles.x);

//    camera->setNearClipDistance(0.01f);
//    camera->setFarClipDistance(100.0f);
//    camera->setOrientation(-p0.orientation);
//    fovy = atanf(1.0f / 2.0f) * 2.0f; // 90.0f / 180.0f * sgl::PI;//
//    camera->setFOVy(fovy);
//    camera->setPosition(p0.position);
    //camera->setPosition(glm::vec3(0.5f, 0.5f, 20.0f));

//    camera->overwriteViewMatrix(glm::toMat4(point[0].orientation) * sgl::matrixTranslation(-point[0].position));

//    update(0.0f);
}




void PixelSyncApp::loadModel(const std::string &filename, bool resetCamera)
{
    // Pure filename without extension (to create compressed .binmesh filename)
    modelFilenamePure = FileUtils::get()->removeExtension(filename);

    if (oitRenderer->isTestingMode()) {
        return;
    }

    lineRadius = 0.001;

    if (timeCoherence)
    {
        if (boost::starts_with(modelFilenamePure, "Data/Rings")) {
            lineRadius = 0.002;
        } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output")) {
            lineRadius = 0.001;
        } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
            lineRadius = 0.0005;
        } else  if (boost::starts_with(modelFilenamePure, "Data/UCLA")) {
            if (timeCoherence) { lineRadius = 0.0005; }
            else { lineRadius = 0.0008; }
        }
    }

    if (recording || testCameraFlight) {
//        if (boost::starts_with(modelFilenamePure, "Data/Rings")) {
//            lineRadius = 0.002;
//        } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output")) {
//            lineRadius = 0.001;
//        } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
//            lineRadius = 0.0005;
//        } else  if (boost::starts_with(modelFilenamePure, "Data/UCLA")) {
//            lineRadius = 0.0008;
        if (boost::starts_with(modelFilenamePure, "Data/CFD/driven_cavity")) {
            lineRadius = 0.0045;
        } else if (boost::starts_with(modelFilenamePure, "Data/CFD/rayleigh")) {
            lineRadius = 0.002;
        } //else {
//            lineRadius = 0.0007;
//        }
    } else {
        if (boost::starts_with(modelFilenamePure, "Data/CFD/driven_cavity")) {
            lineRadius = 0.0045;
        } else if (boost::starts_with(modelFilenamePure, "Data/CFD/rayleigh")) {
            lineRadius = 0.002;
        }
    }

    std::cout << "Line radius = " << lineRadius << std::endl << std::flush;

    // Only load new TF if loading dataset-specific transfer functions isn't overwritten.
    if (transferFunctionName.empty()) {
        if (boost::starts_with(modelFilenamePure, "Data/Rings/test") && !perfMeasurementMode)
        {
            transferFunctionWindow.loadFunctionFromFile(
                    "Data/TransferFunctions/TestRings.xml");
        }
        else if (boost::starts_with(modelFilenamePure, "Data/Trajectories") && !perfMeasurementMode)
        {
            transferFunctionWindow.loadFunctionFromFile(
                    "Data/TransferFunctions/quality/9213_streamlines_semi.xml");
        }
        else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence80000") && !perfMeasurementMode)
        {
            transferFunctionWindow.loadFunctionFromFile(
                    "Data/TransferFunctions/quality/turbulence80000_semi.xml");
        }
        else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output") && !perfMeasurementMode)
        {
            transferFunctionWindow.loadFunctionFromFile(
                    "Data/TransferFunctions/quality/output_semi.xml");
        }
        else if (boost::starts_with(modelFilenamePure, "Data/UCLA") && !perfMeasurementMode)
        {
            transferFunctionWindow.loadFunctionFromFile(
//                    "Data/TransferFunctions/quality/UCLA_400k_100v_semi.xml");
                    "Data/TransferFunctions/UCLA_semi2.xml");
        }
//        if (boost::starts_with(modelFilenamePure, "Data/Rings") && perfMeasurementMode) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/rings_paper.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/Rings") && !perfMeasurementMode) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/rings.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/UCLA") && !perfMeasurementMode) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/UCLA.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories") && perfMeasurementMode) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/9213_streamlines_paper.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence20000")) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/ConvectionRolls01.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence80000")) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/turbulence80000_paper.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/WCB")) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/WCB01.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output")) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/quality/output_semi.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/CFD/driven_cavity")) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/DrivenCavity.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/CFD/rayleigh")) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/RayleighBenard.xml");
//        } else if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
//            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/Hair.xml");
        else {
            transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/Standard.xml");
        }
    }

    if (boost::starts_with(modelFilenamePure, "Data/Rings/test"))
    {
        lineRadius = 0.010;
    }


    if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output")) {
        sgl::ShaderManager->addPreprocessorDefine("CONVECTION_ROLLS", "");
    } else {
        sgl::ShaderManager->removePreprocessorDefine("CONVECTION_ROLLS");
    }

    modelType = MODEL_TYPE_TRIANGLE_MESH_NORMAL;

    bool modelContainsTrajectories = boost::starts_with(modelFilenamePure, "Data/Trajectories")
            || boost::starts_with(modelFilenamePure, "Data/Rings")
            || boost::starts_with(modelFilenamePure, "Data/Turbulence")
            || boost::starts_with(modelFilenamePure, "Data/WCB")
            || boost::starts_with(modelFilenamePure, "Data/ConvectionRolls")
            || boost::starts_with(modelFilenamePure, "Data/UCLA")
            || boost::starts_with(modelFilenamePure, "Data/CFD");
    if (modelContainsTrajectories) {
        modelType = MODEL_TYPE_TRAJECTORIES;
    }

    if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
        modelType = MODEL_TYPE_HAIR;
    }

    if (boost::starts_with(modelFilenamePure, "Data/Models")
            || boost::starts_with(modelFilenamePure, "Data/IsoSurfaces")) {
        modelType = MODEL_TYPE_TRIANGLE_MESH_NORMAL;
    }

    if (boost::starts_with(modelFilenamePure, "Data/IsoSurfaces")) {
        modelType = MODEL_TYPE_TRIANGLE_MESH_SCIENTIFIC;
    }

    if (boost::starts_with(modelFilenamePure, "Data/PointDatasets")) {
        modelType = MODEL_TYPE_POINTS;
    }

    if (modelType == MODEL_TYPE_TRAJECTORIES) {
        if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
            trajectoryType = TRAJECTORY_TYPE_ANEURYSM;
        } else if (boost::starts_with(modelFilenamePure, "Data/WCB"))
        {
            trajectoryType = TRAJECTORY_TYPE_WCB;
        } else if (boost::starts_with(modelFilenamePure, "Data/Rings")) {
            trajectoryType = TRAJECTORY_TYPE_RINGS;
        } else if (boost::starts_with(modelFilenamePure, "Data/UCLA")) {
            trajectoryType = TRAJECTORY_TYPE_UCLA;
        } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output")) {
            trajectoryType = TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW;
        } else if (boost::starts_with(modelFilenamePure, "Data/CFD")) {
            trajectoryType = TRAJECTORY_TYPE_CFD;
        } else {
            trajectoryType = TRAJECTORY_TYPE_CONVECTION_ROLLS;
        }
        changeImportanceCriterionType();
    }

    std::string modelFilenameOptimized = modelFilenamePure + ".binmesh";
    // Special mode for line trajectories: Trajectories loaded as line set or as triangle mesh
    if (modelType == MODEL_TYPE_TRAJECTORIES && lineRenderingTechnique == LINE_RENDERING_TECHNIQUE_LINES) {
        modelFilenameOptimized += "_lines";
        if (useBillboardLines) {
            sgl::ShaderManager->addPreprocessorDefine("BILLBOARD_LINES", "");
        }
        useGeometryShader = true;
    } else {
        // Remove billboard line define before switching to using no geometry shader
        if (useGeometryShader && useBillboardLines) {
            sgl::ShaderManager->removePreprocessorDefine("BILLBOARD_LINES");
        }
        useGeometryShader = false;
    }
    if (modelType == MODEL_TYPE_TRAJECTORIES && lineRenderingTechnique == LINE_RENDERING_TECHNIQUE_FETCH) {
        modelFilenameOptimized += "_lines";
        useProgrammableFetch = true;
        sgl::ShaderManager->addPreprocessorDefine("USE_PROGRAMMABLE_FETCH", "");
        if (programmableFetchUseAoS) {
            sgl::ShaderManager->addPreprocessorDefine("PROGRAMMABLE_FETCH_ARRAY_OF_STRUCTS", "");
        } else {
            sgl::ShaderManager->removePreprocessorDefine("PROGRAMMABLE_FETCH_ARRAY_OF_STRUCTS");
        }
    } else {
        useProgrammableFetch = false;
        sgl::ShaderManager->removePreprocessorDefine("USE_PROGRAMMABLE_FETCH");
    }

    if (!FileUtils::get()->exists(modelFilenameOptimized)) {
        if (modelType == MODEL_TYPE_TRIANGLE_MESH_NORMAL) {
            convertObjMeshToBinary(filename, modelFilenameOptimized);
        } else if (modelType == MODEL_TYPE_TRAJECTORIES) {
            if (boost::ends_with(modelFilenameOptimized, "_lines")) {
                convertTrajectoryDataToBinaryLineMesh(trajectoryType, filename, modelFilenameOptimized);
            } else {
                convertTrajectoryDataToBinaryTriangleMesh(trajectoryType, filename,
                        modelFilenameOptimized, lineRadius);
//                convertTrajectoryDataToBinaryTriangleMeshGPU(trajectoryType, filename,
//                        modelFilenameOptimized, lineRadius);
            }
        } else if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            convertHairDataToBinaryTriangleMesh(filename, modelFilenameOptimized);
        } else if (boost::starts_with(modelFilenamePure, "Data/IsoSurfaces")) {
            convertBinaryObjMeshToBinmesh(filename, modelFilenameOptimized);
        } else if (boost::starts_with(modelFilenamePure, "Data/PointDatasets")) {
            convertPointDataSetToBinmesh(filename, modelFilenameOptimized);
        }
    }

    if (boost::starts_with(modelFilenamePure, "Data/IsoSurfaces")) {
        if (cullBackface) {
            cullBackface = false;
            glDisable(GL_CULL_FACE);
        }
    } else {
        if (!cullBackface) {
            cullBackface = true;
            glEnable(GL_CULL_FACE);
        }
    }

    if (modelType == MODEL_TYPE_TRIANGLE_MESH_NORMAL) {
        gatherShaderIDs = {"PseudoPhong.Vertex", "PseudoPhong.Fragment"};
    } else if (modelType == MODEL_TYPE_TRAJECTORIES) {
        if (boost::ends_with(modelFilenameOptimized, "_lines")) {
            if (!useProgrammableFetch) {
                gatherShaderIDs = {"PseudoPhongTrajectories.Vertex", "PseudoPhongTrajectories.Geometry",
                                   "PseudoPhongTrajectories.Fragment"};
            } else {
                gatherShaderIDs = {"PseudoPhongTrajectories.FetchVertex", "PseudoPhongTrajectories.Fragment"};
            }
        } else {
            gatherShaderIDs = {"PseudoPhongTrajectories.TriangleVertex", "PseudoPhongTrajectories.Fragment"};
        }
    } else if (modelType == MODEL_TYPE_HAIR) {
        gatherShaderIDs = {"PseudoPhongHair.Vertex", "PseudoPhongHair.Fragment"};
    } else if (modelType == MODEL_TYPE_TRIANGLE_MESH_SCIENTIFIC) {
        gatherShaderIDs = {"PseudoPhongTriangleScientific.Vertex", "PseudoPhongTriangleScientific.Fragment"};
    } else if (modelType == MODEL_TYPE_POINTS) {
        gatherShaderIDs = {"PseudoPhongPoints.Vertex", "PseudoPhongPoints.Geometry", "PseudoPhongPoints.Fragment"};
    }

    updateShaderMode(SHADER_MODE_UPDATE_NEW_MODEL);

    if (mode != RENDER_MODE_VOXEL_RAYTRACING_LINES && mode != RENDER_MODE_RAYTRACING) {
        transparentObject = parseMesh3D(modelFilenameOptimized, transparencyShader, shuffleGeometry,
                useProgrammableFetch, programmableFetchUseAoS, lineRadius);
        if (shaderMode == SHADER_MODE_SCIENTIFIC_ATTRIBUTE) {
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
    } else if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        transparentObject = parseMesh3D(modelFilenameOptimized, transparencyShader, shuffleGeometry,
                useProgrammableFetch, programmableFetchUseAoS);
        boundingBox = transparentObject.boundingBox;
        std::vector<float> lineAttributes;
        OIT_VoxelRaytracing *voxelRaytracer = (OIT_VoxelRaytracing*)oitRenderer.get();
        float maxVorticity = 0.0f;
        voxelRaytracer->loadModel(usedModelIndex, trajectoryType, lineAttributes, maxVorticity);
        // Hair stores own line thickness
        if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            lineRadius = voxelRaytracer->getLineRadius();
        }
        transferFunctionWindow.computeHistogram(lineAttributes, 0.0f, maxVorticity);
        transparentObject = MeshRenderer();
#ifdef USE_RAYTRACING
    } else if (mode == RENDER_MODE_RAYTRACING) {
        transparentObject = parseMesh3D(modelFilenameOptimized, transparencyShader, shuffleGeometry,
                useProgrammableFetch, programmableFetchUseAoS);
        boundingBox = transparentObject.boundingBox;
        std::vector<float> lineAttributes;
        OIT_RayTracing *raytracer = (OIT_RayTracing*)oitRenderer.get();
        float maxVorticity = 0.0f;
        bool useTriangleMesh = lineRenderingTechnique == LINE_RENDERING_TECHNIQUE_TRIANGLES;
        raytracer->loadModel(usedModelIndex, trajectoryType, useTriangleMesh/*, lineAttributes, maxVorticity*/);
        // Hair stores own line thickness
        if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            lineRadius = raytracer->getLineRadius();
        }
        transferFunctionWindow.computeHistogram(lineAttributes, 0.0f, maxVorticity);
        transparentObject = MeshRenderer();
#endif
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
        if (shaderMode != SHADER_MODE_SCIENTIFIC_ATTRIBUTE && !boost::starts_with(modelFilenamePure, "Data/Hair")) {
            transparencyShader->setUniform("bandedColorShading", 1);
        }

        if (resetCamera) {
            if (modelFilenamePure == "Data/Models/dragon") {
                camera->setPosition(glm::vec3(0.15f, 0.8f, 2.4f));
            } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories/single_streamline")) {
                camera->setPosition(glm::vec3(0.72f, 0.215f, 0.2f));
                // TODO
                //ControlPoint(1, 0.723282, 0.22395, 0.079609, -4.74008, -0.35447),
                camera->setPosition(glm::vec3(0.723282f, 0.22395f, 0.079609f));
                camera->setYaw(-4.74008f);
                camera->setPitch(0.079609f);
                //ControlPoint(0, 0.604353, 0.223011, 0.144089, -5.23073, 0.0232946),
                camera->setPosition(glm::vec3(0.604353f, 0.223011f, 0.144089f));
                camera->setYaw(-5.23073f);
                camera->setPitch(0.0232946f);
            } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories/torus")) {
                // ControlPoint(1, 1.94571, 0.761162, 2.9094, -1.61776, -0.0535428)
                camera->setPosition(glm::vec3(1.94571f, 0.761162f, 2.9094f));
                camera->setYaw(-1.61776f);
                camera->setPitch(-0.0535428f);
            } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
                camera->setPosition(glm::vec3(0.3f, 0.325f, 1.005f));
            } else if (boost::starts_with(modelFilenamePure, "Data/WCB")) {
                // ControlPoint(1, 1.1286, 0.639969, 0.0575997, -2.98384, -0.411015),
                camera->setPosition(glm::vec3(1.1286f, 0.639969f, 0.0575997f));
                camera->setYaw(-2.98384f);
                camera->setPitch(-0.411015f);
            } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence20000")) {
                // ControlPoint(0, 1.13824, 0.369338, 0.937429, -2.58216, -0.0342094),
                camera->setPosition(glm::vec3(0.468071f, 0.324327f, 0.50661f));
            } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence80000")) {
                // ControlPoint(0, 1.13824, 0.369338, 0.937429, -2.58216, -0.0342094),
                camera->setPosition(glm::vec3(1.13824f, 0.369338f, 0.937429f));
                camera->setYaw(-2.58216f);
                camera->setPitch(-0.0342094f);
                //camera->setPosition(glm::vec3(0.3f, 0.325f, 1.005f));
            } else if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
                //camera->setPosition(glm::vec3(0.6f, 0.4f, 1.8f));
                camera->setPosition(glm::vec3(0.3f, 0.325f, 1.005f));
            } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output")) {
                //camera->setPosition(glm::vec3(0.6f, 0.4f, 1.8f));
                camera->setPosition(glm::vec3(0.0290112, 0.579268, 1.06786));
                camera->setYaw(-1.56575f);
                camera->setPitch(-0.643149f);
            } else if (boost::starts_with(modelFilenamePure, "Data/CFD/driven_cavity")) {
                //ControlPoint(0, -0.497002, -0.326444, 1.06027, -1.51539, -0.126569),
                camera->setPosition(glm::vec3(-0.497002f, -0.326444f, 1.06027f));
                camera->setYaw(-1.51539f);
                camera->setPitch(-0.126569f);
            } else if (boost::starts_with(modelFilenamePure, "Data/CFD/rayleigh")) {
                //ControlPoint(0, -0.508684, -1.85755, -0.273644, -1.5651, -0.0865055),
                camera->setPosition(glm::vec3(-0.508684f, -1.85755f, -0.273644f));
                camera->setYaw(-1.5651f);
                camera->setPitch(-0.0865055f);
            } else if (boost::starts_with(modelFilenamePure, "Data/Rings/test")) {
                // ControlPoint(1, 0.695618, 0.516083, 0.737828, 4.76855, 0.252483),
                camera->setPosition(glm::vec3( 0.695618, 0.516083, 0.737828));
                camera->setYaw(4.76855);
                camera->setPitch(0.252483);
            } else if (boost::starts_with(modelFilenamePure, "Data/Rings")) {
                // ControlPoint(1, 0.154441, 0.0162448, 0.483843, -1.58799, 0.101394),
                camera->setPosition(glm::vec3(0.154441f, 0.0162448f, 0.483843f));
            } else if (boost::starts_with(modelFilenamePure, "Data/IsoSurfaces")) {
                // ControlPoint(1, 0.154441, 0.0162448, 0.483843, -1.58799, 0.101394),
//                camera->setPosition(glm::vec3(1023, 1023, 977));
                camera->setPosition(glm::vec3(0, 0.5, 0));
            } else if (boost::starts_with(modelFilenamePure, "Data/UCLA")) {
                // ControlPoint(1, 0.154441, 0.0162448, 0.483843, -1.58799, 0.101394),
//                camera->setPosition(glm::vec3(1023, 1023, 977));
                camera->setPosition(glm::vec3(0.5, 0.5, 1.6));
            } else {
                camera->setPosition(glm::vec3(0.0f, -0.1f, 2.4f));
            }
        }
        if (modelFilenamePure == "Data/Models/dragon") {
            const float scalingFactor = 0.2f;
            scaling = matrixScaling(glm::vec3(scalingFactor));
        }
    }


    boundingBox = boundingBox.transformed(rotation * scaling);
    updateAOMode();
    shadowTechnique->newModelLoaded(modelFilenamePure, modelContainsTrajectories);
    shadowTechnique->setLightDirection(lightDirection, boundingBox);

    if (testCameraFlight) {
        std::string cameraPathFilename = "Data/CameraPaths/"
                + sgl::FileUtils::get()->getPathAsList(modelFilenamePure).back() + ".binpath";
        //if (sgl::FileUtils::get()->exists(cameraPathFilename)) {
        //    cameraPath.fromBinaryFile(cameraPathFilename);
        //} else {
        cameraPath.fromCirclePath(boundingBox, modelFilenamePure);
        cameraPath.saveToBinaryFile(cameraPathFilename);
        //}
    }

    reRender = true;
}

void PixelSyncApp::changeImportanceCriterionType()
{
    if (trajectoryType == TRAJECTORY_TYPE_ANEURYSM) {
        importanceCriterionIndex = (int)importanceCriterionTypeAneurysm;
    } else if (trajectoryType == TRAJECTORY_TYPE_WCB) {
        importanceCriterionIndex = (int)importanceCriterionTypeWCB;
    } else if (trajectoryType == TRAJECTORY_TYPE_CFD) {
        importanceCriterionIndex = (int)importanceCriterionTypeCFD;
    } else if (trajectoryType == TRAJECTORY_TYPE_UCLA) {
        importanceCriterionIndex = (int)importanceCriterionTypeUCLA;
    } else {
        importanceCriterionIndex = (int)importanceCriterionTypeConvectionRolls;
    }
    ShaderManager->addPreprocessorDefine("IMPORTANCE_CRITERION_INDEX", importanceCriterionIndex);
}

void PixelSyncApp::recomputeHistogramForMesh()
{
    ImportanceCriterionAttribute importanceCriterionAttribute =
            transparentObject.importanceCriterionAttributes.at(importanceCriterionIndex);
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
#ifdef USE_RAYTRACING
    } else if (mode == RENDER_MODE_RAYTRACING) {
        oitRenderer = boost::shared_ptr<OIT_Renderer>(new OIT_RayTracing(camera, clearColor));
#endif
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

    if ((oldMode == RENDER_MODE_VOXEL_RAYTRACING_LINES || oldMode == RENDER_MODE_RAYTRACING)
        && mode != RENDER_MODE_VOXEL_RAYTRACING_LINES && mode != RENDER_MODE_RAYTRACING) {
        loadModel(MODEL_FILENAMES[usedModelIndex], false);
    }
    if (oldMode == RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE) {
        loadModel(MODEL_FILENAMES[usedModelIndex], true);
    }

    if (transparentObject.isLoaded() && mode != RENDER_MODE_VOXEL_RAYTRACING_LINES && mode != RENDER_MODE_RAYTRACING
            && !oitRenderer->isTestingMode()) {
        transparentObject.setNewShader(transparencyShader);
        if (shaderMode != SHADER_MODE_SCIENTIFIC_ATTRIBUTE) {
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
        voxelRaytracer->loadModel(usedModelIndex, trajectoryType, lineAttributes, maxVorticity);
        // Hair stores own line thickness
        if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            lineRadius = voxelRaytracer->getLineRadius();
        }
        transferFunctionWindow.computeHistogram(lineAttributes, 0.0f, maxVorticity);
    }

#ifdef USE_RAYTRACING
    if (modelFilenamePure.length() > 0 && mode == RENDER_MODE_RAYTRACING) {
        std::vector<float> lineAttributes;
        OIT_RayTracing *raytracer = (OIT_RayTracing*)oitRenderer.get();
        float maxVorticity = 0.0f;
        bool useTriangleMesh = lineRenderingTechnique == LINE_RENDERING_TECHNIQUE_TRIANGLES;
        raytracer->loadModel(usedModelIndex, trajectoryType, useTriangleMesh/*, lineAttributes, maxVorticity*/);
        // Hair stores own line thickness
        if (boost::starts_with(modelFilenamePure, "Data/Hair")) {
            lineRadius = raytracer->getLineRadius();
        }
        transferFunctionWindow.computeHistogram(lineAttributes, 0.0f, maxVorticity);
    }
#endif

    if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        modelFilenamePure = FileUtils::get()->removeExtension(MODEL_FILENAMES[usedModelIndex]);

        OIT_VoxelRaytracing *voxelRaytracer = (OIT_VoxelRaytracing*)oitRenderer.get();
        voxelRaytracer->setLineRadius(lineRadius);
        voxelRaytracer->setClearColor(clearColor);
        voxelRaytracer->setLightDirection(lightDirection);
        voxelRaytracer->setTransferFunctionTexture(transferFunctionWindow.getTransferFunctionMapTexture());
    }

#ifdef USE_RAYTRACING
    if (mode == RENDER_MODE_RAYTRACING) {
        modelFilenamePure = FileUtils::get()->removeExtension(MODEL_FILENAMES[usedModelIndex]);

        if (recording || testCameraFlight) {
            if (boost::starts_with(modelFilenamePure, "Data/Rings")) {
                lineRadius = 0.002;
            } else if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output")) {
                lineRadius = 0.001;
            } else if (boost::starts_with(modelFilenamePure, "Data/Trajectories")) {
                lineRadius = 0.0005;
            } else {
                lineRadius = 0.0007;
            }
        }

        OIT_RayTracing *raytracer = (OIT_RayTracing*)oitRenderer.get();
        raytracer->setLineRadius(lineRadius);
        raytracer->setClearColor(clearColor);
        raytracer->setLightDirection(lightDirection);
    }
#endif


    clearColorSelection = ImColor(255, 255, 255, 255);
    if (mode == RENDER_MODE_OIT_DEPTH_COMPLEXITY) {
        OIT_DepthComplexity *depthComplexityOIT = (OIT_DepthComplexity*)oitRenderer.get();
        if (recording) {
            depthComplexityOIT->setRecordingMode(true);
        }
        if (perfMeasurementMode && measurer != nullptr) {
            depthComplexityOIT->setPerfMeasurer(measurer);
        }

        clearColorSelection = ImColor(0, 0, 0, 255);
    }
    clearColor = colorFromFloat(clearColorSelection.x, clearColorSelection.y, clearColorSelection.z,
                                clearColorSelection.w);
    if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
        static_cast<OIT_VoxelRaytracing*>(oitRenderer.get())->setClearColor(clearColor);
#ifdef USE_RAYTRACING
    } else if (mode == RENDER_MODE_RAYTRACING) {
        static_cast<OIT_RayTracing*>(oitRenderer.get())->setClearColor(clearColor);
#endif
    }
    transferFunctionWindow.setClearColor(clearColor);



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
    // TODO: SHADER_MODE_UPDATE_EFFECT_CHANGE
    if (modelType == MODEL_TYPE_TRAJECTORIES || modelType == MODEL_TYPE_TRIANGLE_MESH_SCIENTIFIC
            || modelType == MODEL_TYPE_POINTS) {
        if (shaderMode != SHADER_MODE_SCIENTIFIC_ATTRIBUTE || modeUpdate == SHADER_MODE_UPDATE_NEW_OIT_RENDERER
            || modeUpdate == SHADER_MODE_UPDATE_EFFECT_CHANGE) {
            shaderMode = SHADER_MODE_SCIENTIFIC_ATTRIBUTE;
        }
    } else {
        if (shaderMode == SHADER_MODE_SCIENTIFIC_ATTRIBUTE || modeUpdate == SHADER_MODE_UPDATE_EFFECT_CHANGE) {
            shaderMode = SHADER_MODE_PSEUDO_PHONG;
        }
    }
}

void PixelSyncApp::setNewState(const InternalState &newState)
{
    // 0. Change the resolution?
    Window *window = AppSettings::get()->getMainWindow();
    int currentWindowWidth = window->getWidth();
    int currentWindowHeight = window->getHeight();
    glm::ivec2 newResolution = newState.windowResolution;
    if (newResolution.x > 0 && newResolution.x > 0 && currentWindowWidth != newResolution.x
            && currentWindowHeight != newResolution.y) {
        window->setWindowSize(newResolution.x, newResolution.y);
    }

    // 1.1. Test whether fragment shader invocation interlock (Pixel Sync) or atomic operations shall be disabled
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
    if (newState.testPixelSyncUnordered) {
        ShaderManager->addPreprocessorDefine("PIXEL_SYNC_UNORDERED", "");
    } else {
        ShaderManager->removePreprocessorDefine("PIXEL_SYNC_UNORDERED");
    }

    // 1.2. Set the new line rendering mode
    LineRenderingTechnique oldLineRenderingTechnique = lineRenderingTechnique;
    lineRenderingTechnique = newState.lineRenderingTechnique;

    // 2.1. Handle global state changes like ambient occlusion, shadowing, tiling mode
    setNewTilingMode(newState.tilingWidth, newState.tilingHeight, newState.useMortonCodeForTiling);
    bool shallReloadShader = false;
    if (currentAOTechnique != newState.aoTechniqueName) {
        currentAOTechnique = newState.aoTechniqueName;
        updateAOMode();
    }
    if (currentShadowTechnique != newState.shadowTechniqueName) {
        shallReloadShader = true;
        currentShadowTechnique = newState.shadowTechniqueName;
        updateShadowMode();
    }
    if (currentAOTechnique != newState.aoTechniqueName || currentShadowTechnique != newState.shadowTechniqueName) {
        shallReloadShader = true;
        ShaderManager->invalidateShaderCache();
        updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
    }
    if (shallReloadShader) {
        ShaderManager->invalidateShaderCache();
        updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
    }

    // 2.2. If trajectory model: Load new transfer function if necessary
    if (!newState.transferFunctionName.empty() && this->transferFunctionName != newState.transferFunctionName) {
        transferFunctionWindow.loadFunctionFromFile("Data/TransferFunctions/" + newState.transferFunctionName);
        this->transferFunctionName = newState.transferFunctionName;
    }

    // 3.1. Load right model file
    std::string modelFilename = "";
    int newModelIndex = -1;
    for (int i = 0; i < NUM_MODELS; i++) {
        if (MODEL_DISPLAYNAMES[i] == newState.modelName) {
            modelFilename = MODEL_FILENAMES[i];
            newModelIndex = i;
        }
    }

    if (modelFilename.length() < 1) {
        Logfile::get()->writeError(std::string() + "Error in PixelSyncApp::setNewState: Invalid model name \""
                + newState.modelName + + "\".");
        exit(1);
    }
    if (newModelIndex != usedModelIndex || shuffleGeometry != newState.testShuffleGeometry
            || oldLineRenderingTechnique != lineRenderingTechnique) {
        shuffleGeometry = newState.testShuffleGeometry;
        loadModel(modelFilename);
    }
    usedModelIndex = newModelIndex;

    // 3.2. If trajectory model: Set correct importance criterion
    if (modelType == MODEL_TYPE_TRAJECTORIES && importanceCriterionIndex != newState.importanceCriterionIndex) {
        if (trajectoryType == TRAJECTORY_TYPE_ANEURYSM) {
            importanceCriterionTypeAneurysm = (ImportanceCriterionTypeAneurysm)newState.importanceCriterionIndex;
        }
        else if (trajectoryType == TRAJECTORY_TYPE_UCLA) {
            importanceCriterionTypeUCLA = (ImportanceCriterionTypeUCLA)newState.importanceCriterionIndex;
        }
        else if (trajectoryType == TRAJECTORY_TYPE_WCB) {
            importanceCriterionTypeWCB = (ImportanceCriterionTypeWCB)newState.importanceCriterionIndex;
        } else if (trajectoryType == TRAJECTORY_TYPE_CFD) {
            importanceCriterionTypeCFD = (ImportanceCriterionTypeCFD)newState.importanceCriterionIndex;
        } else {
            importanceCriterionTypeConvectionRolls
                    = (ImportanceCriterionTypeConvectionRolls)newState.importanceCriterionIndex;
        }
        changeImportanceCriterionType();
        recomputeHistogramForMesh();
        ShaderManager->invalidateShaderCache();
        updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
        transparentObject.setNewShader(transparencyShader);
        reRender = true;
    }

    // 4. Set OIT algorithm
    setRenderMode(newState.oitAlgorithm, true);

    // 5. Pass state change to OIT mode to handle internally necessary state changes.
    if (firstState || newState != lastState) {
        oitRenderer->setNewState(newState);
    }

    recordingTime = 0.0f;
    recordingTimeLast = 0.0f;
    recordingTimeStampStart = Timer->getTicksMicroseconds();
    lastState = newState;
    firstState = false;
    usesNewState = true;
    frameNum = 0;


    FRAME_TIME = 0.5f;

    if (perfMeasurementMode && !timeCoherence && mode == RENDER_MODE_OIT_DEPTH_PEELING) {
        if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/turbulence80000")
            || boost::starts_with(modelFilenamePure, "Data/WCB")) {
            FRAME_TIME = 17.0f;
        }
    }
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
        voxelAOHelper->loadAOFactorsFromVoxelFile(modelFilenamePure, trajectoryType);
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
        videoWriter = new VideoWriter("video.mp4", 25);
    }


    reRender = reRender || oitRenderer->needsReRender() || oitRenderer->isTestingMode();
    // reRender = true;

    GLsync fence;

    if (continuousRendering || reRender) {
        renderOIT();
        reRender = false;
        Renderer->unbindFBO();
    }

    if ((perfMeasurementMode && timeCoherence) || recording)
    {
        fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
    }


    //glDisable(GL_FRAMEBUFFER_SRGB);

    if (mode != RENDER_MODE_RAYTRACING) {
        // Render to screen
        Renderer->setProjectionMatrix(matrixIdentity());
        Renderer->setViewMatrix(matrixIdentity());
        Renderer->setModelMatrix(matrixIdentity());
        if (useLinearRGB) {
            Renderer->blitTexture(sceneTexture, AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)),
                                  gammaCorrectionShader);
        } else {
            Renderer->blitTexture(sceneTexture, AABB2(glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)));
        }
#ifdef USE_RAYTRACING
    } else {
        static_cast<OIT_RayTracing*>(oitRenderer.get())->blitTexture();
#endif
    }

    if (perfMeasurementMode) {// && frameNum == 0) {

        if (timeCoherence || recording) {
            bool renderingComplete = false;
            while(!renderingComplete) {
                auto signal = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);

                if (signal == GL_ALREADY_SIGNALED || signal == GL_CONDITION_SATISFIED) {
                    std::cout << "[INFO]: Frame rendering complete" << std::endl << std::flush;
                    renderingComplete = true;
                }
                else
                {
                    if (signal == GL_WAIT_FAILED)
                    {
                        std::cerr << "[ERROR]: wait for rendering failed" << std::endl << std::flush;
                        exit(-1);
                    }
                    if (signal == GL_TIMEOUT_EXPIRED)
                    {
                        std::cerr << "[WARNING]: timeout has expired" << std::endl << std::flush;
                        continue;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            // Delete fence
            glDeleteSync(fence);
            measurer->makeScreenshot(frameNum);
        }

        if (frameNum == 0) {
            measurer->makeScreenshot();
        }

        frameNum++;
    }

    if (!uiOnScreenshot && screenshot) {
        printNow = true;
        makeScreenshot();
        printNow = false;
    }

    // Video recording enabled?
    if (recording) {
        //Renderer->unbindFBO();
        bool renderingComplete = false;
        while(!renderingComplete) {
            auto signal = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);

            if (signal == GL_ALREADY_SIGNALED || signal == GL_CONDITION_SATISFIED) {
//                std::cout << "[INFO]: Frame rendering complete" << std::endl << std::flush;
                renderingComplete = true;
            }
            else
            {
                if (signal == GL_WAIT_FAILED)
                {
                    std::cerr << "[ERROR]: wait for rendering failed" << std::endl << std::flush;
                    exit(-1);
                }
                if (signal == GL_TIMEOUT_EXPIRED)
                {
                    std::cerr << "[WARNING]: timeout has expired" << std::endl << std::flush;
                    continue;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Delete fence
        glDeleteSync(fence);
        videoWriter->pushWindowFrame();
        //Renderer->bindFBO(sceneFramebuffer);
    }

    renderGUI();
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
            measurer->startMeasure(recordingTimeLast);
        }

        Renderer->bindFBO(sceneFramebuffer);
        oitRenderer->renderToScreen();

        if (perfMeasurementMode) {
            measurer->endMeasure();
        }
#endif
        return;
    }

    if (mode == RENDER_MODE_RAYTRACING) {
#ifdef PROFILING_MODE
        oitRenderer->renderToScreen();
#else
        if (perfMeasurementMode) {
            measurer->startMeasure(recordingTimeLast);
        }

        Renderer->bindFBO(sceneFramebuffer);
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
    timer.startGPU("gatherBegin");
    oitRenderer->gatherBegin();
    timer.end();

    timer.startGPU("renderScene");
    oitRenderer->renderScene();
    timer.end();

    timer.startGPU("gatherEnd");
    oitRenderer->gatherEnd();
    timer.end();

    timer.startGPU("renderToScreen");
    oitRenderer->renderToScreen();
    timer.end();
#else
    if (perfMeasurementMode) {
        measurer->startMeasure(recordingTimeLast);
    }

    oitRenderer->gatherBegin();
    oitRenderer->renderScene();
    oitRenderer->gatherEnd();

    oitRenderer->renderToScreen();

    if (perfMeasurementMode) {
        measurer->endMeasure();
    }
#endif

    // Render light direction sphere
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

            if (ImGui::Combo("Rendering Mode", (int*)&lineRenderingTechnique, LINE_RENDERING_TECHNIQUE_DISPLAYNAMES,
                    IM_ARRAYSIZE(LINE_RENDERING_TECHNIQUE_DISPLAYNAMES))) {
                loadModel(MODEL_FILENAMES[usedModelIndex], false);
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
        if (transferFunctionWindow.getTransferFunctionMapRebuilt()) {
            if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
                static_cast<OIT_VoxelRaytracing*>(oitRenderer.get())->onTransferFunctionMapRebuilt();
#ifdef USE_RAYTRACING
            } else if (mode == RENDER_MODE_RAYTRACING) {
                static_cast<OIT_RayTracing*>(oitRenderer.get())->onTransferFunctionMapRebuilt();
#endif
            }
        }
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
#ifdef USE_RAYTRACING
        } else if (mode == RENDER_MODE_RAYTRACING) {
            static_cast<OIT_RayTracing*>(oitRenderer.get())->setClearColor(clearColor);
#endif
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
#ifdef USE_RAYTRACING
        } else if (mode == RENDER_MODE_RAYTRACING) {
            static_cast<OIT_RayTracing*>(oitRenderer.get())->setLightDirection(lightDirection);
#endif
        }
        if (currentShadowTechnique != NO_SHADOW_MAPPING) {
            shadowTechnique->setLightDirection(lightDirection, boundingBox);
        }
        reRender = true;
    }

    // FPS
    //ImGui::PlotLines("Frame Times", &fpsArray.front(), fpsArray.size(), fpsArrayOffset);

    if (ImGui::Checkbox("Cull Back Face", &cullBackface)) {
        if (cullBackface) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        reRender = true;
    } ImGui::SameLine();
    ImGui::Checkbox("Continuous Rendering", &continuousRendering);
    ImGui::Checkbox("UI on Screenshot", &uiOnScreenshot);

    if (shaderMode == SHADER_MODE_SCIENTIFIC_ATTRIBUTE || modelType == MODEL_TYPE_HAIR) {
        ImGui::SameLine();
        if (ImGui::Checkbox("Transparency", &transparencyMapping)) {
            reRender = true;
        }
        if (ImGui::Checkbox("Color By Position", &colorByPosition)) {
            reRender = true;
        }
        ImGui::Checkbox("Show Transfer Function Window", &transferFunctionWindow.getShowTransferFunctionWindow());

        if (ImGui::Checkbox("Use Linear RGB", &useLinearRGB)) {
            updateColorSpaceMode();
            reRender = true;
        }

//        if (boost::starts_with(modelFilenamePure, "Data/Rings"))
//        {
//            lineRadius = 0.0025;
//        }
//        if (boost::starts_with(modelFilenamePure, "Data/ConvectionRolls/output"))
//        {
//            lineRadius = 0.0015;
//        }

        if (useGeometryShader) {
            if (ImGui::Checkbox("Billboard Lines", &useBillboardLines)) {
                if (useBillboardLines) {
                    sgl::ShaderManager->addPreprocessorDefine("BILLBOARD_LINES", "");
                } else {
                    sgl::ShaderManager->removePreprocessorDefine("BILLBOARD_LINES");
                }
                updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
                reRender = true;
            }
        }
        if (useProgrammableFetch) {
            ImGui::SameLine();
            if (ImGui::Checkbox("AoS", &programmableFetchUseAoS)) {
                if (programmableFetchUseAoS) {
                    sgl::ShaderManager->addPreprocessorDefine("PROGRAMMABLE_FETCH_ARRAY_OF_STRUCTS", "");
                } else {
                    sgl::ShaderManager->removePreprocessorDefine("PROGRAMMABLE_FETCH_ARRAY_OF_STRUCTS");
                }
                updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
                loadModel(MODEL_FILENAMES[usedModelIndex], false);
                reRender = true;
            }
        }
        if ((useGeometryShader || useProgrammableFetch || mode == RENDER_MODE_VOXEL_RAYTRACING_LINES)
            && ImGui::SliderFloat("Line radius", &lineRadius, 0.0001f, 0.01f, "%.4f")) {
            if (mode == RENDER_MODE_VOXEL_RAYTRACING_LINES) {
                static_cast<OIT_VoxelRaytracing *>(oitRenderer.get())->setLineRadius(lineRadius);
#ifdef USE_RAYTRACING
            } else if (mode == RENDER_MODE_RAYTRACING) {
                static_cast<OIT_RayTracing*>(oitRenderer.get())->setLineRadius(lineRadius);
#endif
            }
            reRender = true;
        }
        if (modelType == MODEL_TYPE_POINTS
            && ImGui::SliderFloat("Point radius", &pointRadius, 0.00005f, 0.005f, "%.5f")) {
            reRender = true;
        }
    }

    if (shaderMode == SHADER_MODE_SCIENTIFIC_ATTRIBUTE) {
        // Switch importance criterion
        if (mode != RENDER_MODE_VOXEL_RAYTRACING_LINES
                && ((trajectoryType == TRAJECTORY_TYPE_ANEURYSM
                && ImGui::Combo("Importance Criterion", (int*)&importanceCriterionTypeAneurysm,
                                IMPORTANCE_CRITERION_ANEURYSM_DISPLAYNAMES,
                                IM_ARRAYSIZE(IMPORTANCE_CRITERION_ANEURYSM_DISPLAYNAMES)))
                || (trajectoryType == TRAJECTORY_TYPE_WCB
                && ImGui::Combo("Importance Criterion", (int*)&importanceCriterionTypeWCB,
                                IMPORTANCE_CRITERION_WCB_DISPLAYNAMES,
                                IM_ARRAYSIZE(IMPORTANCE_CRITERION_WCB_DISPLAYNAMES)))
                || ((trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS || trajectoryType == TRAJECTORY_TYPE_CONVECTION_ROLLS_NEW || trajectoryType == TRAJECTORY_TYPE_RINGS)
                && ImGui::Combo("Importance Criterion", (int*)&importanceCriterionTypeConvectionRolls,
                                IMPORTANCE_CRITERION_CONVECTION_ROLLS_DISPLAYNAMES,
                                IM_ARRAYSIZE(IMPORTANCE_CRITERION_CONVECTION_ROLLS_DISPLAYNAMES)))
                || ((trajectoryType == TRAJECTORY_TYPE_CFD)
                && ImGui::Combo("Importance Criterion", (int*)&importanceCriterionTypeCFD,
                                IMPORTANCE_CRITERION_CFD_DISPLAYNAMES,
                                IM_ARRAYSIZE(IMPORTANCE_CRITERION_CFD_DISPLAYNAMES))))) {
            changeImportanceCriterionType();
            recomputeHistogramForMesh();
            ShaderManager->invalidateShaderCache();
            updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
            transparentObject.setNewShader(transparencyShader);
            reRender = true;
        }
    }

    if (ImGui::Combo("AO Mode", (int*)&currentAOTechnique, AO_TECHNIQUE_DISPLAYNAMES,
                     IM_ARRAYSIZE(AO_TECHNIQUE_DISPLAYNAMES))) {
        ShaderManager->invalidateShaderCache();
        updateAOMode();
        updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
        reRender = true;
    }

    if (ImGui::SliderFloat("AO Factor", &aoFactor, 0.0f, 1.0f)) {
        reRender = true;
    }

    if (ImGui::Combo("Shadow Mode", (int*)&currentShadowTechnique, SHADOW_MAPPING_TECHNIQUE_DISPLAYNAMES,
                     IM_ARRAYSIZE(SHADOW_MAPPING_TECHNIQUE_DISPLAYNAMES))) {
        ShaderManager->invalidateShaderCache();
        updateShadowMode();
        updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
        reRender = true;
    }

    if (ImGui::SliderFloat("Shadow Factor", &shadowFactor, 0.0f, 1.0f)) {
        reRender = true;
    }

    if (ImGui::Combo("Reflection Model", (int*)&reflectionModelType, REFLECTION_MODEL_DISPLAY_NAMES,
                     IM_ARRAYSIZE(REFLECTION_MODEL_DISPLAY_NAMES))) {
        ShaderManager->addPreprocessorDefine("REFLECTION_MODEL", (int)reflectionModelType);
        ShaderManager->invalidateShaderCache();
        updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
        reRender = true;
    }

    if (shadowTechnique->renderGUI()) {
        reRender = true;

        // Needs new transparency shader because of changed global settings?
        if (shadowTechnique->getNeedsNewTransparencyShader()) {
            updateShaderMode(SHADER_MODE_UPDATE_EFFECT_CHANGE);
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

//    ImVec2 cursorPosEnd = ImGui::GetCursorPos(); ImGui::SameLine();

    ImGui::Separator();

    std::string testText = "Test";
    ImGui::InputText("##savecameralabel", &saveFilename);
    if (ImGui::Button("Save camera")) {
        saveCameraPositionToFile(saveDirectory + saveFilename);
    }
    ImGui::SameLine();

    if (ImGui::Button("Load camera")) {
        loadCameraPositionFromFile(saveDirectory + saveFilename + ".camera");
        reRender = true;
    }

    ImGui::InputText("##savescreenshotlabel", &saveFilenameScreenshots);
    if (ImGui::Button("Save screenshot")) {
        saveScreenshotOnKey(saveDirectoryScreenshots + saveFilenameScreenshots + ".png");
    }
}

sgl::ShaderProgramPtr PixelSyncApp::setUniformValues()
{
    ShaderProgramPtr transparencyShader;
    bool isHairDataset = boost::starts_with(modelFilenamePure, "Data/Hair");

    if ((currentAOTechnique != AO_TECHNIQUE_SSAO || !ssaoHelper->isPreRenderPass())
            && (currentShadowTechnique == NO_SHADOW_MAPPING || !shadowTechnique->isShadowMapCreatePass())) {
        transparencyShader = oitRenderer->getGatherShader();
        if (shaderMode == SHADER_MODE_SCIENTIFIC_ATTRIBUTE) {
            transparencyShader->setUniform("minCriterionValue", minCriterionValue);
            transparencyShader->setUniform("maxCriterionValue", maxCriterionValue);
            if (transparencyShader->hasUniform("radius")) {
                if (modelType == MODEL_TYPE_POINTS) {
                    transparencyShader->setUniform("radius", pointRadius);
                } else {
                    transparencyShader->setUniform("radius", lineRadius);
                }
            }
            transparencyShader->setUniform("transparencyMapping", transparencyMapping);
            if (transparencyShader->hasUniform("colorByPosition"))
            {
                transparencyShader->setUniform("colorByPosition", colorByPosition);
            }

            transparencyShader->setUniform("transferFunctionTexture",
                    transferFunctionWindow.getTransferFunctionMapTexture(), 5);
            //transparencyShader->setUniformBuffer(2, "TransferFunctionBlock",
            //      transferFunctionWindow.getTransferFunctionMapUBO());
            //std::cout << "Max Vorticity: " << *maxVorticity << std::endl;
            //transparencyShader->setUniform("cameraPosition", -camera->getPosition());
        }
        if (transparencyShader->hasUniform("cameraPosition")) {
            transparencyShader->setUniform("cameraPosition", camera->getPosition());
        }

        if (shaderMode != SHADER_MODE_SCIENTIFIC_ATTRIBUTE) {
            // Hack for supporting multiple passes...
            if (modelFilenamePure == "Data/Models/Ship_04") {
                transparencyShader->setUniform("bandedColorShading", 0);
            } else if (boost::starts_with(modelFilenamePure, "Data/IsoSurfaces"))
            {
                transparencyShader->setUniform("bandedColorShading", 0);
            }
            else if (!isHairDataset) {
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
            if (shaderMode == SHADER_MODE_SCIENTIFIC_ATTRIBUTE) {
                transparencyShader->setUniform("minCriterionValue", minCriterionValue);
                transparencyShader->setUniform("maxCriterionValue", maxCriterionValue);
                if (transparencyShader->hasUniform("radius")) {
                    transparencyShader->setUniform("radius", lineRadius);
                }
                transparencyShader->setUniform("transparencyMapping", transparencyMapping);
                if (transparencyShader->hasUniform("colorByPosition"))
                {
                    transparencyShader->setUniform("colorByPosition", colorByPosition);
                }

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
            && reflectionModelType != LOCAL_SHADOW_MAP_OCCLUSION
            && transparencyShader->hasUniform("aoFactorGlobal")) {
        transparencyShader->setUniform("aoFactorGlobal", aoFactor);
    }
    if (currentShadowTechnique != NO_SHADOW_MAPPING && !shadowTechnique->isShadowMapCreatePass()
            && reflectionModelType != AMBIENT_OCCLUSION_FACTOR
            && transparencyShader->hasUniform("shadowFactorGlobal")) {
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
    transparentObject.render(transparencyShader, isGBufferPass, importanceCriterionIndex);
}


void PixelSyncApp::processSDLEvent(const SDL_Event &event)
{
    ImGuiWrapper::get()->processSDLEvent(event);
}


void PixelSyncApp::update(float dt)
{
    AppLogic::update(dt);

//    std::cout << dt << std::endl << std::flush;
    //dt = 1/60.0f;

    //std::cout << "dt: " << dt << std::endl;

    //std::cout << ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) << std::endl;
    //std::cout << ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) << std::endl;
    fpsArrayOffset = (fpsArrayOffset + 1) % fpsArray.size();
    fpsArray[fpsArrayOffset] = 1.0f/dt;

    recordingTimeLast = recordingTime;

    if (perfMeasurementMode && !measurer->update(recordingTime)) {
        // All modes were tested -> quit
        quit();
    }

    if (recording || testCameraFlight || perfMeasurementMode) {
        cameraPath.update(recordingTime);
        camera->overwriteViewMatrix(cameraPath.getViewMatrix());

        reRender = true;
    }

    if (perfMeasurementMode || recording || testCameraFlight) {
        // Already recorded full cycle?
        if (recording && recordingTime > cameraPath.getEndTime()) {
            quit();
        }

        // Otherwise, update camera position
        if (realTimeCameraFlight) {
            uint64_t currentTimeStamp = Timer->getTicksMicroseconds();
            uint64_t timeElapsedMicroSec = currentTimeStamp - recordingTimeStampStart;
            recordingTime = timeElapsedMicroSec * 1e-6;
            if (usesNewState) {
                // A new state was just set. Don't recompute, as this would result in time of ca. 1-2ns
                usesNewState = false;
                recordingTime = 0.0f;
            }
        } else {
            if (perfMeasurementMode) {
                recordingTime += 0.5f;
            } else{
                recordingTime += FRAME_TIME;
            }

        }
    }
    //recordingTime = 0.0f;

    if (Keyboard->keyPressed(SDLK_c)) {
        // ControlPoint(0.0f, 0.3f, 0.325f, 1.005f, 0.0f, 0.0f),
        std::cout << "ControlPoint(" << outputTime << ", " << camera->getPosition().x << ", " << camera->getPosition().y
                << ", " << camera->getPosition().z << ", " << camera->getYaw() << ", " << camera->getPitch()
                << ")," << std::endl;

        saveCameraPosition();

        outputTime += 1.0f;
    }

//    if (Keyboard->keyPressed(SDLK_l)) {
//
//        loadCameraPositionFromFile(saveDirectory + saveFilename + ".camera");
//
//        reRender = true;
//    }


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

    if (Keyboard->isKeyDown(SDLK_p))
    {
//        auto now = std::chrono::system_clock::now();
//        std::time_t end_time = std::chrono::system_clock::to_time_t(now);

        //saveScreenshotOnKey(saveDirectoryScreenshots + saveFilenameScreenshots + "_" + std::to_string(numScreenshots++) + "_mode" + std::to_string(mode) + ".png");
    }

    if (Keyboard->isKeyDown(SDLK_u))
    {
        showSettingsWindow = !showSettingsWindow;
        transferFunctionWindow.setShow(showSettingsWindow);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
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
        float moveAmount = Mouse->getScrollWheel()*dt*2.0;
        camera->translate(transformPoint(invRotationMatrix, glm::vec3(0.0f, 0.0f, -moveAmount*MOVE_SPEED)));
        reRender = true;

    }


    // Mouse rotation
    if (Mouse->isButtonDown(1) && Mouse->mouseMoved()) {
        sgl::Point2 pixelMovement = Mouse->mouseMovement();
        float yaw = dt*MOUSE_ROT_SPEED*pixelMovement.x;
        float pitch = -dt*MOUSE_ROT_SPEED*pixelMovement.y;

        glm::quat rotYaw = glm::quat(glm::vec3(0.0f, yaw, 0.0f));
        glm::quat rotPitch = glm::quat(pitch*glm::vec3(rotationMatrix[0][0], rotationMatrix[1][0],
                rotationMatrix[2][0]));
        camera->rotateYaw(yaw);
        camera->rotatePitch(pitch);
        reRender = true;
    }
}
