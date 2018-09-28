//
// Created by christoph on 27.09.18.
//

#include "InternalState.hpp"


std::vector<InternalState> getAllTestModes()
{
    std::vector<InternalState> states;
    InternalState state;

    state.name = "MBOIT 4 Power Moments";
    state.modelName = "Monkey";
    state.oitAlgorithm = RENDER_MODE_OIT_MBOIT;
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "usePowerMoments", "true" },
            { "numMoments", "4" },
            { "accuracy", "UNORM" },
    });
    states.push_back(state);

    state.name = "MLAB 4 Layers";
    state.oitAlgorithm = RENDER_MODE_OIT_MLAB;
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "numLayers", "4" },
    });
    states.push_back(state);

    return states;
}