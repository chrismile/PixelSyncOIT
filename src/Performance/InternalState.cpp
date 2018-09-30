//
// Created by christoph on 27.09.18.
//

#include "../OIT/OIT_LinkedList.hpp"
#include "InternalState.hpp"

void getTestModesNoOIT(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_DUMMY;
    state.name = "No OIT";
    states.push_back(state);
}

void getTestModesMBOIT(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MBOIT;

    for (int useTrigMoments = 0; useTrigMoments < 1; useTrigMoments++) {
        for (int unorm = 0; unorm < 1; unorm++) {
            for (int numMoments = 4; numMoments <= 8; numMoments += 2) {
                state.name = std::string() + "MBOIT " + sgl::toString(numMoments)
                             + (useTrigMoments ? " Trigonometric Moments " : " Power Moments ")
                             + (unorm == 0 ? "Float" : "UNORM");
                state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
                        { "usePowerMoments", (useTrigMoments ? "false" : "true") },
                        { "numMoments", sgl::toString(numMoments) },
                        { "accuracy", (unorm == 0 ? "Float" : "UNORM") },
                });
                states.push_back(state);
            }
        }
    }
}

void getTestModesMLAB(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MLAB;

    for (int numLayers = 1; numLayers <= 32; numLayers *= 2) {
        state.name = std::string() + "MLAB " + sgl::toString(numLayers) + " Layers";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numLayers", sgl::toString(numLayers) },
        });
        states.push_back(state);
    }
}

void getTestModesHT(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_HT;

    for (int numLayers = 1; numLayers <= 32; numLayers *= 2) {
        state.name = std::string() + "HT " + sgl::toString(numLayers) + " Layers";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numLayers", sgl::toString(numLayers) },
        });
        states.push_back(state);
    }
}

void getTestModesKBuffer(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_KBUFFER;

    for (int numLayers = 1; numLayers <= 32; numLayers *= 2) {
        state.name = std::string() + "K-Buffer " + sgl::toString(numLayers) + " Layers";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numLayers", sgl::toString(numLayers) },
        });
        states.push_back(state);
    }
}

#ifndef IM_ARRAYSIZE
#define IM_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR)/sizeof(*_ARR)))
#endif

void getTestModesLinkedList(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_LINKED_LIST;

    // sortingModeStrings
    for (int expectedDepthComplexity = 8; expectedDepthComplexity <= 128; expectedDepthComplexity *= 2) {
        for (int sortingModeIdx = 0; sortingModeIdx < IM_ARRAYSIZE(sortingModeStrings); sortingModeIdx++) {
            for (int maxNumFragmentsSorting = 64; maxNumFragmentsSorting <= 1024; maxNumFragmentsSorting *= 2) {
                std::string sortingMode = sortingModeStrings[sortingModeIdx];
                state.name = std::string() + "Linked List " + sortingMode + + " "
                             + sgl::toString(maxNumFragmentsSorting) + " Layers, "
                             + sgl::toString(expectedDepthComplexity) + " Nodes per Pixel";
                state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                        { "sortingMode", sortingMode },
                        { "maxNumFragmentsSorting", sgl::toString(maxNumFragmentsSorting) },
                        { "expectedDepthComplexity", sgl::toString(expectedDepthComplexity) },
                });
            }
        }
    }

    for (int numLayers = 1; numLayers <= 32; numLayers *= 2) {
        state.name = std::string() + "K-Buffer " + sgl::toString(numLayers) + " Layers";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numLayers", sgl::toString(numLayers) },
        });
        states.push_back(state);
    }
}

void getTestModesDepthPeeling(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_DEPTH_PEELING;
    state.name = std::string() + "Depth Peeling";
    states.push_back(state);
}


std::vector<InternalState> getAllTestModes()
{
    std::vector<InternalState> states;
    InternalState state;
    state.modelName = "Monkey";

    getTestModesDepthPeeling(states, state);
    getTestModesNoOIT(states, state);
    getTestModesMLAB(states, state);
    getTestModesMBOIT(states, state);
    getTestModesHT(states, state);
    getTestModesKBuffer(states, state);
    getTestModesLinkedList(states, state);

    return states;
}