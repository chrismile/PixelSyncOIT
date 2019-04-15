/*
 * MainApp.hpp
 *
 *  Created on: 22.04.2017
 *      Author: Christoph Neuhauser
 */

#ifndef LOGIC_MainApp_HPP_
#define LOGIC_MainApp_HPP_

#include <vector>
#include <glm/glm.hpp>

#include <Utils/AppLogic.hpp>
#include <Utils/Random/Xorshift.hpp>
#include <Math/Geometry/Point2.hpp>
#include <Graphics/Shader/ShaderAttributes.hpp>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Scene/Camera.hpp>
#include <Graphics/OpenGL/TimerGL.hpp>

#include "Utils/VideoWriter.hpp"
#include "Utils/MeshSerializer.hpp"
#include "Utils/CameraPath.hpp"
#include "Utils/ImportanceCriteria.hpp"
#include "OIT/OIT_Renderer.hpp"
#include "AmbientOcclusion/SSAO.hpp"
#include "AmbientOcclusion/VoxelAO.hpp"
#include "Shadows/ShadowMapping.hpp"
#include "Shadows/MomentShadowMapping.hpp"
#include "Performance/InternalState.hpp"
#include "Performance/AutoPerfMeasurer.hpp"
#include "TransferFunctionWindow.hpp"

using namespace std;
using namespace sgl;

//#define PROFILING_MODE

enum ShaderMode {
    SHADER_MODE_PSEUDO_PHONG, SHADER_MODE_VORTICITY, SHADER_MODE_AMBIENT_OCCLUSION
};

class PixelSyncApp : public AppLogic
{
public:
    PixelSyncApp();
    ~PixelSyncApp();
    void render(); // Calls renderOIT and renderGUI
    void renderOIT(); // Uses renderScene and "oitRenderer" to render the scene
    void renderScene(); // Renders lighted scene
    void update(float dt);
    void resolutionChanged(EventPtr event);
    void processSDLEvent(const SDL_Event &event);


protected:
    // State changes
    void setRenderMode(RenderModeOIT newMode, bool forceReset = false);
    enum ShaderModeUpdate {
        SHADER_MODE_UPDATE_NEW_OIT_RENDERER, SHADER_MODE_UPDATE_NEW_MODEL, SHADER_MODE_UPDATE_EFFECT_CHANGE
    };
    void updateShaderMode(ShaderModeUpdate modeUpdate);
    void loadModel(const std::string &filename, bool resetCamera = true);

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

    // Override screenshot function to exclude GUI (if wanted by the user)
    void saveScreenshot(const std::string &filename);


    sgl::ShaderProgramPtr setUniformValues();

private:
    void renderGUI(); // Renders GUI
    void renderSceneSettingsGUI();
    void updateColorSpaceMode();

    // Lighting & rendering
    boost::shared_ptr<Camera> camera;
    float fovy;
    ShaderProgramPtr transparencyShader;
    ShaderProgramPtr gammaCorrectionShader;

    // Screen space ambient occlusion
    SSAOHelper *ssaoHelper = NULL;
    VoxelAOHelper *voxelAOHelper = NULL;
    AOTechniqueName currentAOTechnique = AO_TECHNIQUE_NONE;
    void updateAOMode();

    // Current rendering/shading/lighting model
    ReflectionModelType reflectionModelType = PSEUDO_PHONG_LIGHTING;
    float aoFactor = 0.5f;
    float shadowFactor = 0.5f;

    // Shadow rendering
    boost::shared_ptr<ShadowTechnique> shadowTechnique;
    ShadowMappingTechniqueName currentShadowTechnique = NO_SHADOW_MAPPING;
    void updateShadowMode();

    // Mode
    // RENDER_MODE_VOXEL_RAYTRACING_LINES RENDER_MODE_OIT_MBOIT RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE
    RenderModeOIT mode = RENDER_MODE_OIT_MLAB_BUCKET; // RENDER_MODE_OIT_MLAB RENDER_MODE_OIT_MLAB_BUCKET RENDER_MODE_VOXEL_RAYTRACING_LINES
    RenderModeOIT oldMode = mode;
    ShaderMode shaderMode = SHADER_MODE_PSEUDO_PHONG;
    std::string modelFilenamePure;
    bool shuffleGeometry = false; // For testing order dependency of OIT algorithms on triangle order
    std::list<std::string> gatherShaderIDs;

    // Off-screen rendering
    FramebufferObjectPtr sceneFramebuffer;
    TexturePtr sceneTexture;
    RenderbufferObjectPtr sceneDepthRBO;

    // Objects in the scene
    boost::shared_ptr<OIT_Renderer> oitRenderer;
    MeshRenderer transparentObject;
    glm::mat4 rotation;
    glm::mat4 scaling;
    sgl::AABB3 boundingBox;

    // User interface
    bool showSettingsWindow = true;
    int usedModelIndex = 2; // 2: Aneurism, 11: Ponytail, 16: Turbulence, 18: Convection Rolls
    Color bandingColor;
    Color clearColor;
    ImVec4 clearColorSelection = ImColor(0, 0, 0, 255);
    bool cullBackface = true;
    bool transparencyMapping = true;
    bool useLinearRGB = true;
    float lineRadius = 0.001f;
    std::vector<float> fpsArray;
    size_t fpsArrayOffset = 0;
    glm::vec3 lightDirection = glm::vec3(1.0, 0.0, 0.0);
    bool uiOnScreenshot = false;
    bool printNow = false;
    float MOVE_SPEED = 0.2f;
    float ROT_SPEED = 1.0f;
    float MOUSE_ROT_SPEED = 0.05f;

    TransferFunctionWindow transferFunctionWindow;

    // Trajectory rendering
    bool modelContainsTrajectories;
    bool modelContainsHair;
    std::string transferFunctionName;
    TrajectoryType trajectoryType;
    ImportanceCriterionTypeAneurism importanceCriterionTypeAneurism
            = IMPORTANCE_CRITERION_ANEURISM_VORTICITY;
    ImportanceCriterionTypeWCB importanceCriterionTypeWCB
            = IMPORTANCE_CRITERION_WCB_TOTAL_PRESSURE_DIFFERENCE;
    ImportanceCriterionTypeConvectionRolls importanceCriterionTypeConvectionRolls
            = IMPORTANCE_CRITERION_CONVECTION_ROLLS_VORTICITY;
    int importanceCriterionIndex = 0;
    float minCriterionValue = 0.0f, maxCriterionValue = 1.0f;
    bool usesGeometryShader = false;
    void changeImportanceCriterionType();
    void recomputeHistogramForMesh();

    // Hair rendering
    bool colorArrayMode = false;

    // Continuous rendering: Re-render each frame or only when scene changes?
    bool continuousRendering = false;
    bool reRender = true;

    // Profiling events
    AutoPerfMeasurer *measurer;
    bool perfMeasurementMode = false;
    InternalState lastState;
    bool firstState = true;
    bool usesNewState = true;
    int frameNum = 0;
#ifdef PROFILING_MODE
    sgl::TimerGL timer;
#endif

    // Save video stream to file
    const int FRAME_RATE = 60;
    float FRAME_TIME = 1.0f / FRAME_RATE;
    uint64_t recordingTimeStampStart;
    float recordingTime = 0.0f;
    float recordingTimeLast = 0.0f;

    float outputTime = 0.0f;
    bool testOutputPos = true;
    bool testCameraFlight = false;
    bool realTimeCameraFlight = false;
    bool recordingUseGlobalIlumination = false;
    bool recording = false;
    VideoWriter *videoWriter;

    CameraPath cameraPath;
};

#endif /* LOGIC_MainApp_HPP_ */
