//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_INTERNALSTATE_HPP
#define PIXELSYNCOIT_INTERNALSTATE_HPP

#include <string>
#include <map>
#include <Utils/Convert.hpp>

const int NUM_OIT_MODES = 9;
const char *const OIT_MODE_NAMES[] = {
        "K-Buffer", "Linked List", "Multi-layer Alpha Blending", "Hybrid Transparency", "Moment-Based OIT",
        "Depth Complexity", "No OIT", "Depth Peeling", "Voxel Raytracing (Lines)"
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
    RENDER_MODE_VOXEL_RAYTRACING_LINES
};

const int NUM_MODELS = 8;
const char *const MODEL_FILENAMES[] = {
        "Data/Trajectories/single_streamline", "Data/Trajectories/9213_streamlines", "Data/Models/Ship_04",
        "Data/Models/Monkey", "Data/Models/Box", "Data/Models/Plane", "Data/Models/dragon",
        "Data/Trajectories/lagranto_out"
};
const char *const MODEL_DISPLAYNAMES[] = {
        "Single Streamline", "Streamlines", "Ship", "Monkey", "Box", "Plane", "Dragon", "Lagranto"
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

    void getValueOpt(const char *key, std::string &toset) const { auto it = settings.find(key); if (it != settings.end()) { toset = it->second; } }
    template<typename T> void getValueOpt(const char *key, T &toset) const {
        auto it = settings.find(key);
        if (it != settings.end()) {
            toset = sgl::fromString<T>(it->second);
        }
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
    std::string name;
    std::string modelName;
    RenderModeOIT oitAlgorithm;
    SettingsMap oitAlgorithmSettings;
    int tilingWidth = 2;
    int tilingHeight = 8;
    bool useSSAO = false;
    bool useStencilBuffer = true;
    bool testNoInvocationInterlock = false; // Test without pixel sync
    bool testNoAtomicOperations = false; // Test without atomic operations
};

std::vector<InternalState> getAllTestModes();

#endif //PIXELSYNCOIT_INTERNALSTATE_HPP
