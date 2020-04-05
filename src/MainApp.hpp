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
    SHADER_MODE_PSEUDO_PHONG, SHADER_MODE_SCIENTIFIC_ATTRIBUTE, SHADER_MODE_AMBIENT_OCCLUSION
};

enum ModelType {
    MODEL_TYPE_TRIANGLE_MESH_NORMAL, // Triangle mesh with normal shading
    MODEL_TYPE_TRAJECTORIES, // Trajectory data set (i.e., lines)
    MODEL_TYPE_HAIR, // Hair data set (i.e., lines, but without scalar attribute and transfer function)
    MODEL_TYPE_TRIANGLE_MESH_SCIENTIFIC, // Triangle mesh with color determined by transfer function
    MODEL_TYPE_POINTS // Point data set
};

struct CameraSetting
{
public:
    CameraSetting() {}
    CameraSetting(float time, float tx, float ty, float tz, float yaw_, float pitch_);

    glm::vec3 position;
    float pitch;
    float yaw;
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
    void loadCameraPositionFromFile(const std::string& filename);

    // For changing performance measurement modes
    void setNewState(const InternalState &newState);

    // Override screenshot function to exclude GUI (if wanted by the user)
    void saveScreenshot(const std::string &filename);
    void saveScreenshotOnKey(const std::string &filename);

    void saveCameraPosition();
    void saveCameraPositionToFile(const std::string &filename);



    sgl::ShaderProgramPtr setUniformValues();

private:
    void renderGUI(); // Renders GUI
    void renderSceneSettingsGUI();
    void renderLineRenderingSettingsGUI();
    void renderMultiVarSettingsGUI();
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
    float aoFactor = 1.0f;
    float shadowFactor = 1.0f;

    // Shadow rendering
    boost::shared_ptr<ShadowTechnique> shadowTechnique;
    ShadowMappingTechniqueName currentShadowTechnique = NO_SHADOW_MAPPING;
    void updateShadowMode();

    // Mode
    // RENDER_MODE_VOXEL_RAYTRACING_LINES RENDER_MODE_OIT_MBOIT RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE RENDER_MODE_OIT_MLAB_BUCKET RENDER_MODE_OIT_LINKED_LIST
    RenderModeOIT mode = RENDER_MODE_OIT_DUMMY; // RENDER_MODE_OIT_MLAB RENDER_MODE_OIT_MLAB_BUCKET RENDER_MODE_OIT_LINKED_LIST
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
    int usedModelIndex = 0;
    std::string startupModelName = "Multi-Var";
    LineRenderingTechnique lineRenderingTechnique = LINE_RENDERING_TECHNIQUE_LINES;
    Color bandingColor;
    Color clearColor;
    ImVec4 clearColorSelection = ImColor(0, 0, 0, 255);
    bool cullBackface = true;
    bool useBillboardLines = false;
    bool transparencyMapping = true;
    bool colorByPosition = false;
    bool useLinearRGB = true;
    float lineRadius = 0.001f;
    float pointRadius = 0.0002f;
    std::vector<float> fpsArray;
    size_t fpsArrayOffset = 0;
    glm::vec3 lightDirection = glm::vec3(1.0, 0.0, 0.0);
    bool uiOnScreenshot = false;
    bool printNow = false;
    float MOVE_SPEED = 0.2f;
    float ROT_SPEED = 1.0f;
    float MOUSE_ROT_SPEED = 0.05f;

    // Multi-Variate settings
    int32_t numVariables = 4;
    int32_t maxNumVariables = 6;
    int32_t numLineSegments = 8;
    int32_t numInstances = 12;
    float separatorWidth = 0.15;
    bool mapTubeDiameter = false;

    // Lighting settings
    float materialConstantAmbient = 0.1;
    float materialConstantDiffuse = 0.85;
    float materialConstantSpecular = 0.05;
    float materialConstantSpecularExp = 10;
    bool drawHalo = true;
    float haloFactor = 1.2;

    TransferFunctionWindow transferFunctionWindow;

    // Trajectory rendering
    //bool modelContainsTrajectories;
    //bool modelContainsHair;
    ModelType modelType = MODEL_TYPE_TRAJECTORIES;
    std::string transferFunctionName;
    TrajectoryType trajectoryType;
    ImportanceCriterionTypeAneurysm importanceCriterionTypeAneurysm
            = IMPORTANCE_CRITERION_ANEURYSM_VORTICITY;
    ImportanceCriterionTypeWCB importanceCriterionTypeWCB
            = IMPORTANCE_CRITERION_WCB_CURVATURE;
    ImportanceCriterionTypeConvectionRolls importanceCriterionTypeConvectionRolls
            = IMPORTANCE_CRITERION_CONVECTION_ROLLS_VORTICITY;
    ImportanceCriterionTypeCFD importanceCriterionTypeCFD
            = IMPORTANCE_CRITERION_CFD_CURL;
    ImportanceCriterionTypeUCLA importanceCriterionTypeUCLA
            = IMPORTANCE_CRITERION_UCLA_MAGNITUDE;
    MultiVarRenderModeType  multiVarRenderMode = MULTIVAR_RENDERMODE_ROLLS;
    int importanceCriterionIndex = 0;
    float minCriterionValue = 0.0f, maxCriterionValue = 1.0f;
    std::vector<glm::vec2> criterionsMinMaxValues;
    bool useGeometryShader = false;
    bool useProgrammableFetch = false;
    bool programmableFetchUseAoS = true; // Array of structs
    void changeImportanceCriterionType();
    void setMultiVarShaders();
    void recomputeHistogramForMesh();

    // Hair rendering
    bool colorArrayMode = false;

    // Continuous rendering: Re-render each frame or only when scene changes?
    bool continuousRendering = false;
    bool reRender = true;

    // Profiling events
    AutoPerfMeasurer *measurer;
    bool perfMeasurementMode = false;
    bool timeCoherence = false;
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
    bool testCameraFlight = false;
    bool realTimeCameraFlight = false;
    bool recordingUseGlobalIlumination = false;
    bool recording = false;
    VideoWriter *videoWriter;

    CameraPath cameraPath;

    std::string saveDirectory = "Data/Cameras/";
    std::string saveFilename = "Test";
    std::string saveDirectoryScreenshots = "Data/Screenshots/";
    std::string saveFilenameScreenshots = "Screenshot";
    uint32_t numScreenshots = 0;
};

#endif /* LOGIC_MainApp_HPP_ */
