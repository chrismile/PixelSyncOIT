//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_INTERNALSTATE_HPP
#define PIXELSYNCOIT_INTERNALSTATE_HPP

#include <string>
#include <map>
#include <Utils/Convert.hpp>
#include "../Shadows/ShadowTechnique.hpp"

const int NUM_OIT_MODES = 10;
const char *const OIT_MODE_NAMES[] = {
        "K-Buffer", "Linked List", "Multi-layer Alpha Blending", "Hybrid Transparency", "Moment-Based OIT",
        "Depth Complexity", "No OIT", "Depth Peeling", "MLAB (Buckets)", "Voxel Raytracing (Lines)"
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
    RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE
};

const int NUM_MODELS = 16; // 8 datasets + 8 hair
const char *const MODEL_FILENAMES[] = {
        "Data/Trajectories/single_streamline", "Data/Trajectories/9213_streamlines",
        "Data/Trajectories/9213_streamlines", "Data/Models/Ship_04", "Data/Models/Monkey", "Data/Models/Box",
        "Data/Models/Plane", "Data/Models/dragon", //"Data/Trajectories/lagranto_out",
        "Data/Hair/bear.hair",
        "Data/Hair/blonde.hair",
        "Data/Hair/dark.hair",
        "Data/Hair/ponytail.hair",
        "Data/Hair/straight.hair",
        "Data/Hair/wCurly.hair",
        "Data/Hair/wStraight.hair",
        "Data/Hair/wWavy.hair",
        "Data/WCB/EUR_LL025/20121015_00_lagranto_ensemble_forecast__START_20121019_18.nc",
        "Data/WCB/EUR_LL10/20121015_00_lagranto_ensemble_forecast__START_20121017_06.nc",
        "Data/ConvectionRolls/turbulence80000.obj",
        "Data/ConvectionRolls/turbulence20000.obj",
};
const char *const MODEL_DISPLAYNAMES[] = {
        "Single Streamline", "Aneurism (Lines)", "Aneurism Streamlines", "Ship", "Monkey", "Box", "Plane", "Dragon",
        "Bear", "Blonde", "Dark", "Ponytail", "Straight", "wCurly", "wStraight", "wWavy",
        "Warm Conveyor Belt", "Warm Conveyor Belt (low-res)", "Convection Rolls", "Convection Rolls (low-res)"
};

enum AOTechniqueName {
        AO_TECHNIQUE_NONE = 0, AO_TECHNIQUE_SSAO, AO_TECHNIQUE_VOXEL_AO
};
const char *const AO_TECHNIQUE_DISPLAYNAMES[] = {
        "No Ambient Occlusion", "Screen Space AO", "Voxel Ambient Occlusion"
};

enum ReflectionModelType {
    PSEUDO_PHONG_LIGHTING = 0, COMBINED_SHADOW_MAP_AND_AO, LOCAL_SHADOW_MAP_OCCLUSION, AMBIENT_OCCLUSION_FACTOR
};
const char *const REFLECTION_MODEL_DISPLAY_NAMES[] = {
        "Pseudo Phong Lighting", "Combined Shadow Map and AO",
        "Local Shadow Map Occlusion", "Ambient Occlusion Factor"
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
               && this->importanceCriterionIndex == importanceCriterionIndex
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
    int importanceCriterionIndex = 0;
    bool useStencilBuffer = true;
    bool testNoInvocationInterlock = false; // Test without pixel sync
    bool testNoAtomicOperations = false; // Test without atomic operations
    bool testShuffleGeometry = false;
    bool testPixelSyncOrdered = false;
};

std::vector<InternalState> getAllTestModes();

#endif //PIXELSYNCOIT_INTERNALSTATE_HPP
