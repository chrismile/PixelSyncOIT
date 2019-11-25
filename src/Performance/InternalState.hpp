//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_INTERNALSTATE_HPP
#define PIXELSYNCOIT_INTERNALSTATE_HPP

#include <string>
#include <map>
#include <Utils/Convert.hpp>
#include "../Shadows/ShadowTechnique.hpp"

const int NUM_OIT_MODES = 11;
const char *const OIT_MODE_NAMES[] = {
        "K-Buffer", "Linked List", "Multi-layer Alpha Blending", "Hybrid Transparency", "Moment-Based OIT",
        "Depth Complexity", "No OIT", "Depth Peeling", "MLAB (Buckets)", "Voxel Ray Casting (Lines)", "Ray Tracing"
};
enum RenderModeOIT {
    RENDER_MODE_OIT_KBUFFER = 0,
    RENDER_MODE_OIT_LINKED_LIST,
    RENDER_MODE_OIT_MLAB, // Mutli-layer Alpha Blending
    RENDER_MODE_OIT_HT, // Hybrid Transparency
    RENDER_MODE_OIT_MBOIT, // Moment-Based Order-Independent Transparency
    RENDER_MODE_OIT_DEPTH_COMPLEXITY,
    RENDER_MODE_OIT_DUMMY,
    RENDER_MODE_OIT_DEPTH_PEELING,
    RENDER_MODE_OIT_MLAB_BUCKET,
    RENDER_MODE_VOXEL_RAYTRACING_LINES,
    RENDER_MODE_RAYTRACING,
    RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE
};

const char *const MODEL_FILENAMES[] = {
        "Data/Rings/rings.obj",
        "Data/Trajectories/9213_streamlines.obj",
        "Data/ConvectionRolls/output.obj",
        "Data/ConvectionRolls/turbulence80000.obj",
        "Data/ConvectionRolls/turbulence20000.obj",
        "Data/Hair/ponytail.hair",
        "Data/Trajectories/single_streamline.obj",
        "Data/Trajectories/torus.obj",
        "Data/CFD/driven_cavity-streamlines.binlines",
        "Data/CFD/rayleigh_benard_convection_8-2-1-streamlines.binlines",
        "Data/IsoSurfaces/rm-80-isosurface.bobj",
        "Data/IsoSurfaces/rm-140-isosurface.bobj",
        "Data/PointDatasets/0.000xv000.dat",
        "Data/PointDatasets/0.000xv001.dat",
        "Data/PointDatasets/OFC-wasatch-50Mpps.uda.001/t06002/timestep.xml",
        "Data/Models/Ship_04.obj",

//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121017_06pressureDiff.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121017_06.nc",

//        "Data/Hair/ponytail.hair",
//        "Data/Trajectories/single_streamline", "Data/Trajectories/9213_streamlines",
//        "Data/Trajectories/9213_streamlines", "Data/Models/Ship_04", "Data/Models/Monkey", "Data/Models/Box",
//        "Data/Models/Plane", "Data/Models/dragon", //"Data/Trajectories/lagranto_out",
//        "Data/Hair/bear.hair",
//        "Data/Hair/blonde.hair",
//        "Data/Hair/dark.hair",
//        "Data/Hair/straight.hair",
//        "Data/Hair/wCurly.hair",
//        "Data/Hair/wStraight.hair",
//        "Data/Hair/wWavy.hair",
//        "Data/ConvectionRolls/turbulence80000.obj",
//        "Data/ConvectionRolls/turbulence80000.obj",
//        "Data/ConvectionRolls/turbulence20000.obj",
//        "Data/WCB/20121015_00_lagranto_ensemble_forecast__START_20121019_18.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121017_06.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121017_12.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121017_18.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121018_00.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121018_06.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121018_12.nc",
//        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121018_18.nc",
//        "Data/WCB/EUR_LL10/20121015_12_lagranto_ensemble_forecast__START_20121018_06.nc",
};
const int NUM_MODELS = ((int)(sizeof(MODEL_FILENAMES)/sizeof(*MODEL_FILENAMES)));
const char *const MODEL_DISPLAYNAMES[] = {
        "Rings",
        "Aneurysm",
        "Convection Rolls",
        "Turbulence",
        "Convection Rolls Small",
        "Hair",
        "Single Streamline",
        "Torus",
        "Driven Cavity",
        "Rayleigh-Benard Convection",
        "Iso Surface (rm-80)",
        "Iso Surface (rm-140)",
        "Cosmic Web 0",
        "Cosmic Web 1",
        "Uintah Particle Data Set",
        "Ship",

//        "Ponytail",

//        "Single Streamline", "Aneurism (Lines)", "Aneurism Streamlines", "Ship", "Monkey", "Box", "Plane", "Dragon",
//        "Bear", "Blonde", "Dark", "Ponytail", "Straight", "wCurly", "wStraight", "wWavy",
//        "Turbulence", "Turbulence (Lines)", "Convection Rolls", "Warm Conveyor Belt #1", "Warm Conveyor Belt #2",
//        "Warm Conveyor Belt #3", "Warm Conveyor Belt #4", "Warm Conveyor Belt #5", "Warm Conveyor Belt #6",
//        "Warm Conveyor Belt #7", "Warm Conveyor Belt #8", "Warm Conveyor Belt #9",
};

enum LineRenderingTechnique {
    LINE_RENDERING_TECHNIQUE_TRIANGLES, LINE_RENDERING_TECHNIQUE_LINES, LINE_RENDERING_TECHNIQUE_FETCH
};
const char *const LINE_RENDERING_TECHNIQUE_DISPLAYNAMES[] = {
        "Triangles", "Geometry Shader (Lines)", "Prog. Vertex Fetch (Lines)"
};

enum AOTechniqueName {
        AO_TECHNIQUE_NONE = 0, AO_TECHNIQUE_SSAO, AO_TECHNIQUE_VOXEL_AO
};
const char *const AO_TECHNIQUE_DISPLAYNAMES[] = {
        "No Ambient Occlusion", "Screen Space AO", "Voxel Ambient Occlusion"
};

enum ReflectionModelType {
        PSEUDO_PHONG_LIGHTING = 0, COMBINED_SHADOW_MAP_AND_AO, LOCAL_SHADOW_MAP_OCCLUSION, AMBIENT_OCCLUSION_FACTOR,
        NO_LIGHTING
};
const char *const REFLECTION_MODEL_DISPLAY_NAMES[] = {
        "Pseudo Phong Lighting", "Combined Shadow Map and AO", "Local Shadow Map Occlusion", "Ambient Occlusion Factor",
        "No Lighting"
};


class SettingsMap {
public:
    inline std::string getValue(const char *key) const { auto it = settings.find(key); return it == settings.end() ? "" : it->second; }
    inline int getIntValue(const char *key) const { return sgl::fromString<int>(getValue(key)); }
    inline float getFloatValue(const char *key) const { return sgl::fromString<float>(getValue(key)); }
    inline bool getBoolValue(const char *key) const { std::string val = getValue(key); if (val == "false" || val == "0") return false; return val.length() > 0; }
    inline void addKeyValue(const std::string &key, const std::string &value) { settings[key] = value; }
    template<typename T> inline void addKeyValue(const std::string &key, const T &value) { settings[key] = toString(value); }
    inline void clear() { settings.clear(); }

    bool getValueOpt(const char *key, std::string &toset) const {
        auto it = settings.find(key);
        if (it != settings.end()) {
            toset = it->second;
            return true;
        }
        return false;
    }
    bool getValueOpt(const char *key, bool &toset) const {
        auto it = settings.find(key);
        if (it != settings.end()) {
            toset = (it->second == "true") || (it->second == "1");
            return true;
        }
        return false;
    }
    template<typename T> bool getValueOpt(const char *key, T &toset) const {
        auto it = settings.find(key);
        if (it != settings.end()) {
            toset = sgl::fromString<T>(it->second);
            return true;
        }
        return false;
    }

    void set(std::map<std::string, std::string> stringMap) {
        settings = stringMap;
    }

    const std::map<std::string, std::string> &getMap() const {
        return settings;
    }

    //template<typename T>
    //unsigned const T &operator[](const std::string &key) const { return getValue(key); }
    //unsigned T &operator[](const std::string &key) { return getValue(key); }

private:
    std::map<std::string, std::string> settings;
};

struct InternalState
{
    bool operator==(const InternalState &rhs) const {
        return this->name == rhs.name && this->modelName == rhs.modelName && this->oitAlgorithm == rhs.oitAlgorithm
               && this->oitAlgorithmSettings.getMap() == rhs.oitAlgorithmSettings.getMap()
               && this->tilingWidth == rhs.tilingWidth && this->tilingHeight == rhs.tilingHeight
               && this->aoTechniqueName == rhs.aoTechniqueName && this->shadowTechniqueName == rhs.shadowTechniqueName
               && this->lineRenderingTechnique == rhs.lineRenderingTechnique
               && this->transferFunctionName == transferFunctionName
               && this->importanceCriterionIndex == importanceCriterionIndex
               && this->windowResolution == windowResolution
               && this->useStencilBuffer == rhs.useStencilBuffer
               && this->testNoInvocationInterlock == rhs.testNoInvocationInterlock
               && this->testNoAtomicOperations == rhs.testNoAtomicOperations
               && this->testShuffleGeometry == rhs.testShuffleGeometry;
    }
    bool operator!=(const InternalState &rhs) const {
        return !(*this == rhs);
    }
    std::string name;
    std::string modelName;
    RenderModeOIT oitAlgorithm;
    SettingsMap oitAlgorithmSettings;
    int tilingWidth = 2;
    int tilingHeight = 8;
    bool useMortonCodeForTiling = false;
    AOTechniqueName aoTechniqueName = AO_TECHNIQUE_NONE;
    ShadowMappingTechniqueName shadowTechniqueName = NO_SHADOW_MAPPING;
    LineRenderingTechnique lineRenderingTechnique = LINE_RENDERING_TECHNIQUE_TRIANGLES;
    std::string transferFunctionName;
    int importanceCriterionIndex = 0;
    glm::ivec2 windowResolution = glm::ivec2(0, 0);
    bool useStencilBuffer = true;
    bool testNoInvocationInterlock = false; // Test without pixel sync
    bool testNoAtomicOperations = false; // Test without atomic operations
    bool testShuffleGeometry = false;
    bool testPixelSyncUnordered = true;
};

std::vector<InternalState> getTestModesPaper();
std::vector<InternalState> getAllTestModes();

#endif //PIXELSYNCOIT_INTERNALSTATE_HPP
