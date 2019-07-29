//
// Created by christoph on 27.09.18.
//

#include "../OIT/OIT_LinkedList.hpp"
#include "InternalState.hpp"
#include <Utils/File/FileUtils.hpp>
#include <boost/algorithm/string/predicate.hpp>

void getTestModesNoOIT(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_DUMMY;
    state.name = "No OIT";
    states.push_back(state);
}

void getTestModesMBOIT(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MBOIT;

//    for (int useTrigMoments = 0; useTrigMoments <= 1; useTrigMoments++) {
//        for (int unorm = 0; unorm <= 1; unorm++) {
//            for (int numMoments = 4; numMoments <= 8; numMoments += 2) {
//                state.name = std::string() + "MBOIT " + sgl::toString(numMoments)
//                             + (useTrigMoments ? " Trigonometric Moments " : " Power Moments ")
//                             + (unorm == 0 ? "Float" : "UNORM");
//                state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
//                        { "usePowerMoments", (useTrigMoments ? "false" : "true") },
//                        { "numMoments", sgl::toString(numMoments) },
//                        { "pixelFormat", (unorm == 0 ? "Float" : "UNORM") },
//                });
//                states.push_back(state);
//            }
//        }
//    }
//
    state.name = std::string() + "MBOIT 4 Power Moments Float beta 0.1";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "overestimationBeta", "0.1" },
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(4) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);

    state.name = std::string() + "MBOIT 4 Power Moments Float beta 0.25";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "overestimationBeta", "0.25" },
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(4) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);

    state.name = std::string() + "MBOIT 8 Power Moments Float beta 0.1";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "overestimationBeta", "0.1" },
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(8) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);

    state.name = std::string() + "MBOIT 8 Power Moments Float beta 0.25";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "overestimationBeta", "0.25" },
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(8) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);
}

void getTestModesMLAB(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MLAB;

    for (int numLayers = 4; numLayers <= 8; numLayers *= 2) {
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

void getTestModesLinkedListAll(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_LINKED_LIST;

    // sortingModeStrings
    for (int expectedDepthComplexity = 256; expectedDepthComplexity <= 256; expectedDepthComplexity *= 2) {
        for (int sortingModeIdx = 0; sortingModeIdx < IM_ARRAYSIZE(sortingModeStrings); sortingModeIdx++) {
            for (int maxNumFragmentsSorting = 1024; maxNumFragmentsSorting <= 1024; maxNumFragmentsSorting *= 2) {
                std::string sortingMode = sortingModeStrings[sortingModeIdx];
                state.name = std::string() + "Linked List " + sortingMode + + " "
                             + sgl::toString(maxNumFragmentsSorting) + " Layers, "
                             + sgl::toString(expectedDepthComplexity) + " Nodes per Pixel";
                state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                        { "sortingMode", sortingMode },
                        { "maxNumFragmentsSorting", sgl::toString(maxNumFragmentsSorting) },
                        { "expectedDepthComplexity", sgl::toString(expectedDepthComplexity) },
                });
                states.push_back(state);
            }
        }
    }
}

void getTestModesLinkedList(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_LINKED_LIST;

    int maxNumFragmentsSorting = 256;
    int expectedDepthComplexity = 128;
    if (state.modelName == "Turbulence") {
        // Highest depth complexity measured for this dataset
        maxNumFragmentsSorting = 1024;
        expectedDepthComplexity = 500;
    }
    if (state.modelName == "Convection Rolls") {
        // Highest depth complexity measured for this dataset
        maxNumFragmentsSorting = 512;
        expectedDepthComplexity = 256;
    }

    if (state.modelName == "Warm Conveyor Belts") {
        // Highest depth complexity measured for this dataset
        expectedDepthComplexity = 300;
    }

    for (int sortingModeIdx = 0; sortingModeIdx < IM_ARRAYSIZE(sortingModeStrings); sortingModeIdx++) {
        std::string sortingMode = sortingModeStrings[sortingModeIdx];
        state.name = std::string() + "Linked List " + sortingMode + + " "
                     + sgl::toString(maxNumFragmentsSorting) + " Layers, "
                     + sgl::toString(expectedDepthComplexity) + " Nodes per Pixel";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "sortingMode", sortingMode },
                { "maxNumFragmentsSorting", sgl::toString(maxNumFragmentsSorting) },
                { "expectedDepthComplexity", sgl::toString(expectedDepthComplexity) },
        });
        states.push_back(state);
    }
}

void getTestModesLinkedListQuality(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_LINKED_LIST;

    int maxNumFragmentsSorting = 256;
    int expectedDepthComplexity = 128;
    if (state.modelName == "Turbulence") {
        // Highest depth complexity measured for this dataset
        maxNumFragmentsSorting = 1024;
        expectedDepthComplexity = 500;
    }
    if (state.modelName == "Convection Rolls") {
        // Highest depth complexity measured for this dataset
        maxNumFragmentsSorting = 512;
        expectedDepthComplexity = 256;
    }

    if (state.modelName == "Warm Conveyor Belts") {
        // Highest depth complexity measured for this dataset
        expectedDepthComplexity = 300;
    }

    int sortingModeIdx = 0;
    std::string sortingMode = sortingModeStrings[sortingModeIdx];
    state.name = std::string() + "Linked List " + sortingMode + + " "
                 + sgl::toString(maxNumFragmentsSorting) + " Layers, "
                 + sgl::toString(expectedDepthComplexity) + " Nodes per Pixel";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "sortingMode", sortingMode },
            { "maxNumFragmentsSorting", sgl::toString(maxNumFragmentsSorting) },
            { "expectedDepthComplexity", sgl::toString(expectedDepthComplexity) },
    });
    states.push_back(state);
}

void getTestModesDepthPeeling(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_DEPTH_PEELING;
    state.name = std::string() + "Depth Peeling";
    states.push_back(state);
}


// Test: No atomic operations, no pixel sync. Performance difference?
void getTestModesNoSync(std::vector<InternalState> &states, InternalState state)
{
    state.testNoInvocationInterlock = true;
    state.testNoAtomicOperations = true;

    state.oitAlgorithm = RENDER_MODE_OIT_MLAB;
    for (int numLayers = 1; numLayers <= 32; numLayers *= 2) {
        state.name = std::string() + "MLAB " + sgl::toString(numLayers) + " Layers, No Sync";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numLayers", sgl::toString(numLayers) },
        });
        states.push_back(state);
    }


    state.oitAlgorithm = RENDER_MODE_OIT_MBOIT;
    state.name = std::string() + "MBOIT " + sgl::toString(4) + " Power Moments Float, No Sync";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(4) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);


    /*state.oitAlgorithm = RENDER_MODE_OIT_LINKED_LIST;
    state.name = std::string() + "Linked List Priority Queue "
                 + sgl::toString(1024) + " Layers, "
                 + sgl::toString(32) + " Nodes per Pixel, No Atomic Operations";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "sortingMode", "Priority Queue" },
            { "maxNumFragmentsSorting", sgl::toString(1024) },
            { "expectedDepthComplexity", sgl::toString(32) },
    });
    states.push_back(state);*/
}


// Test: Pixel sync using
void getTestModesUnorderedSync(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MLAB;
    for (int numLayers = 1; numLayers <= 32; numLayers *= 2) {
        state.name = std::string() + "MLAB " + sgl::toString(numLayers) + " Layers (Unordered)";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numLayers", sgl::toString(numLayers) },
        });
        states.push_back(state);
    }


    state.oitAlgorithm = RENDER_MODE_OIT_MBOIT;
    state.name = std::string() + "MBOIT " + sgl::toString(4) + " Power Moments Float (Unordered)";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(4) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);
}


// Test: Different tiling modes for different algorithms
void getTestModesTiling(std::vector<InternalState> &states, InternalState state)
{
    std::string tilingString = std::string() + sgl::toString(state.tilingWidth)
            + "x" + sgl::toString(state.tilingHeight);
    if (state.useMortonCodeForTiling) {
        tilingString += " Morton";
    }

    state.oitAlgorithm = RENDER_MODE_OIT_MLAB;
    state.name = std::string() + "MLAB " + sgl::toString(8) + " Layers, Tiling " + tilingString;
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "numLayers", sgl::toString(8) },
    });
    states.push_back(state);


    /*state.oitAlgorithm = RENDER_MODE_OIT_MBOIT;
    state.name = std::string() + "MBOIT " + sgl::toString(4) + " Power Moments Float, Tiling " + tilingString;
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(4) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);*/
}


// Quality test: Shuffle geometry randomly
void getTestModesShuffleGeometry(std::vector<InternalState> &states, InternalState state, int runNumber)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MLAB;
    state.name = std::string() + "MLAB " + sgl::toString(8) + " Layers, Shuffled " + sgl::toString(runNumber);
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "numLayers", sgl::toString(8) },
    });
    states.push_back(state);


    state.oitAlgorithm = RENDER_MODE_OIT_MBOIT;
    state.name = std::string() + "MBOIT " + sgl::toString(4) + " Power Moments Float, Shuffled " + sgl::toString(runNumber);
    state.oitAlgorithmSettings.set(std::map<std::string, std::string> {
            { "usePowerMoments", "true" },
            { "numMoments", sgl::toString(4) },
            { "pixelFormat", "Float" },
    });
    states.push_back(state);


    state.oitAlgorithm = RENDER_MODE_OIT_HT;
    state.name = std::string() + "HT " + sgl::toString(8) + " Layers, Shuffled " + sgl::toString(runNumber);
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "numLayers", sgl::toString(8) },
    });
    states.push_back(state);


    state.oitAlgorithm = RENDER_MODE_OIT_KBUFFER;
    state.name = std::string() + "K-Buffer " + sgl::toString(8) + " Layers, Shuffled " + sgl::toString(runNumber);
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "numLayers", sgl::toString(8) },
    });
    states.push_back(state);


    state.oitAlgorithm = RENDER_MODE_OIT_LINKED_LIST;
    state.name = std::string() + "Linked List Priority Queue "
                 + sgl::toString(1024) + " Layers, "
                 + sgl::toString(32) + " Nodes per Pixel, Shuffled " + sgl::toString(runNumber);
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "sortingMode", "Priority Queue" },
            { "maxNumFragmentsSorting", sgl::toString(512) },
            { "expectedDepthComplexity", sgl::toString(32) },
    });
    states.push_back(state);
}


// Performance test: Pixel sync vs. atomic operations
void getTestModesPixelSyncVsAtomicOps(std::vector<InternalState> &states, InternalState state)
{
    state.testPixelSyncUnordered = true;
    state.oitAlgorithm = RENDER_MODE_TEST_PIXEL_SYNC_PERFORMANCE;
    state.name = std::string() + "Pixel Sync Performance Test Compute (Unordered)";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "testMode", "usePixelSync" },
            { "testType", "compute" },
    });
    states.push_back(state);
    state.testPixelSyncUnordered = false;
    state.name = std::string() + "Pixel Sync Performance Test Compute (Ordered)";
    states.push_back(state);
    state.testPixelSyncUnordered = true;


    state.name = std::string() + "Atomic Operations Performance Test Compute";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "testMode", "useAtomicOps" },
            { "testType", "compute" },
    });
    states.push_back(state);

    state.name = std::string() + "No Synchronization Performance Test Compute";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "testMode", "noSync" },
            { "testType", "compute" },
    });
    states.push_back(state);


    state.testPixelSyncUnordered = true;
    state.name = std::string() + "Pixel Sync Performance Test Sum (Unordered)";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "testMode", "usePixelSync" },
            { "testType", "sum" },
    });
    states.push_back(state);
    state.testPixelSyncUnordered = false;
    state.name = std::string() + "Pixel Sync Performance Test Sum (Ordered)";
    states.push_back(state);
    state.testPixelSyncUnordered = true;

    state.name = std::string() + "Atomic Operations Performance Test Sum";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "testMode", "useAtomicOps" },
            { "testType", "sum" },
    });
    states.push_back(state);

    state.name = std::string() + "No Synchronization Performance Test Sum";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "testMode", "noSync" },
            { "testType", "sum" },
    });
    states.push_back(state);
}

void getTestModesMLABBucketsAll(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MLAB_BUCKET;

    for (int bucketMode = 0; bucketMode < 5; bucketMode++) {
        state.name = std::string() + "MLAB Buckets 4x4 Mode " + sgl::toString(bucketMode) + "";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numBuckets", sgl::toString(4) },
                { "nodesPerBucket", sgl::toString(4) },
                { "bucketMode", sgl::toString(bucketMode) },
        });
        states.push_back(state);
    }

    for (int nodesPerBucket = 2; nodesPerBucket <= 8; nodesPerBucket *= 2) {
        state.name = std::string() + "MLAB Min Depth Buckets " + sgl::toString(nodesPerBucket) + " Layers";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numBuckets", sgl::toString(1) },
                { "nodesPerBucket", sgl::toString(nodesPerBucket) },
                { "bucketMode", sgl::toString(4) },
        });
        states.push_back(state);
    }
}


void getTestModesMLABBuckets(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_MLAB_BUCKET;

    for (int nodesPerBucket = 4; nodesPerBucket <= 8; nodesPerBucket *= 2) {
        state.name = std::string() + "MLAB Min Depth Buckets " + sgl::toString(nodesPerBucket) + " Layers";
        state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
                { "numBuckets", sgl::toString(1) },
                { "nodesPerBucket", sgl::toString(nodesPerBucket) },
                { "bucketMode", sgl::toString(4) },
        });
        states.push_back(state);
    }
}


void getTestModesVoxelRaytracing(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_VOXEL_RAYTRACING_LINES;

    //!TODO set resolution depending on file

    int gridResolution = 128;

    if (boost::starts_with(state.modelName, "Data/Rings")) { gridResolution = 64; }
    if (boost::starts_with(state.modelName, "Data/ConvectionRolls/output")) { gridResolution = 256; }

//    for (int gridResolution = 128; gridResolution <= 128; gridResolution *= 2) {
    state.name = std::string() + "Voxel Ray Casting (Grid " + sgl::toString(gridResolution) + ", Quantization 64, Neighbor Search)";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "gridResolution", sgl::toString(gridResolution) },
            { "quantizationResolution", sgl::toString(64) },
            { "useNeighborSearch", "true" },
    });
    states.push_back(state);
    state.name = std::string() + "Voxel Ray Casting (Grid " + sgl::toString(gridResolution) + ", Quantization 64, No Neighbor Search)";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "gridResolution", sgl::toString(gridResolution) },
            { "quantizationResolution", sgl::toString(64) },
            { "useNeighborSearch", "false" },
    });
    states.push_back(state);
//    }
}

void getTestModesRayTracing(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_RAYTRACING;
    state.name = std::string() + "Ray Tracing (Tubes)";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "useEmbreeCurves", "false" },
    });
    states.push_back(state);

    state.name = std::string() + "Ray Tracing (Embree)";
    state.oitAlgorithmSettings.set(std::map<std::string, std::string>{
            { "useEmbreeCurves", "true" },
    });
    states.push_back(state);
}

void getTestModesDepthComplexity(std::vector<InternalState> &states, InternalState state)
{
    state.oitAlgorithm = RENDER_MODE_OIT_DEPTH_COMPLEXITY;
    state.name = std::string() + "Depth Complexity";
    states.push_back(state);
}


void getTestModesPaperForMesh(std::vector<InternalState> &states, InternalState state)
{
    getTestModesDepthPeeling(states, state);
//    getTestModesNoOIT(states, state);
    getTestModesMLAB(states, state);
    getTestModesMBOIT(states, state);
    getTestModesLinkedList(states, state);
    getTestModesMLABBuckets(states, state);
    getTestModesVoxelRaytracing(states, state);
//    getTestModesDepthComplexity(states, state);
}

void getTestModesPaperForMeshQuality(std::vector<InternalState> &states, InternalState state)
{
    /*getTestModesDepthPeeling(states, state);
//    getTestModesNoOIT(states, state);
    getTestModesMLAB(states, state);
    getTestModesMBOIT(states, state);
    getTestModesLinkedListQuality(states, state);
    getTestModesMLABBuckets(states, state);
    getTestModesVoxelRaytracing(states, state);
//    getTestModesDepthComplexity(states, state);
*/
    getTestModesDepthPeeling(states, state);
    getTestModesLinkedListQuality(states, state);
    getTestModesRayTracing(states, state);
}

void getTestModesPaperForRTPerformance(std::vector<InternalState> &states, InternalState state)
{
    getTestModesRayTracing(states, state);
}

std::vector<InternalState> getTestModesPaper()
{
    std::vector<InternalState> states;
//    std::vector<glm::ivec2> windowResolutions = { glm::ivec2(1280, 720), glm::ivec2(1920, 1080), glm::ivec2(2560, 1440) };
    std::vector<glm::ivec2> windowResolutions = { glm::ivec2(1920, 1080) };
//    std::vector<glm::ivec2> windowResolutions = { glm::ivec2(1280, 720) };
//    std::vector<std::string> modelNames = { "Rings", "Aneurysm", "Turbulence", "Convection Rolls", "Hair" };
//    std::vector<std::string> modelNames = { "Rings", "Aneurysm", "Turbulence", "Convection Rolls" };
    std::vector<std::string> modelNames = { "Rings"/*, "Aneurysm", "Turbulence", "Convection Rolls" */};
    InternalState state;

    for (size_t i = 0; i < windowResolutions.size(); i++) {
        state.windowResolution = windowResolutions.at(i);
        for (size_t j = 0; j < modelNames.size(); j++) {
            state.modelName = modelNames.at(j);
            getTestModesPaperForRTPerformance(states, state);
        }
    }

    for (InternalState &state : states) {
        state.lineRenderingTechnique = LINE_RENDERING_TECHNIQUE_LINES;
    }

    // Append model name to state name if more than one model is loaded
    if (modelNames.size() > 1 || windowResolutions.size() > 1) {
        for (InternalState &state : states) {
            state.name = sgl::toString(state.windowResolution.x) + "x" + sgl::toString(state.windowResolution.y)
                    + " " + state.modelName + " " + state.name;
        }
    }


    // Use different transfer functions?
    std::vector<std::string> transferFunctionNameSuffices = { "Semi", "Full", "High" };
    size_t n = states.size();
    std::vector<InternalState> oldStates = states;
    states.clear();
    for (size_t i = 0; i < oldStates.size(); i++) {
        for (int j = 0; j < transferFunctionNameSuffices.size(); j++) {
            InternalState state = oldStates.at(i);
            std::string modelFilename;
            for (int i = 0; i < NUM_MODELS; i++) {
                if (MODEL_DISPLAYNAMES[i] == state.modelName) {
                    modelFilename = MODEL_FILENAMES[i];
                }
            }
            std::string modelNamePure =
                    sgl::FileUtils::get()->getPureFilename(sgl::FileUtils::get()->removeExtension(modelFilename));
            std::string tfSuffix = transferFunctionNameSuffices.at(j);
            state.name = state.name + "(TF: " + tfSuffix + ")";
            state.transferFunctionName = std::string() + "tests/" + modelNamePure + "_" + tfSuffix + ".xml";
            states.push_back(state);
        }
    }

    return states;
}

std::vector<InternalState> getAllTestModes()
{
    std::vector<InternalState> states;
    InternalState state;
    //state.modelName = "Monkey";
    //state.modelName = "Streamlines";
    state.modelName = "Aneurism Streamlines";

    getTestModesDepthPeeling(states, state);
    getTestModesNoOIT(states, state);
    getTestModesMLAB(states, state);
    getTestModesMBOIT(states, state);
    getTestModesHT(states, state);
    getTestModesKBuffer(states, state);
    getTestModesLinkedList(states, state);

    // Performance test: Different values for tiling
    InternalState stateTiling = state;
    stateTiling.tilingWidth = 1;
    stateTiling.tilingHeight = 1;
    getTestModesTiling(states, stateTiling);
    stateTiling.tilingWidth = 2;
    stateTiling.tilingHeight = 2;
    getTestModesTiling(states, stateTiling);
    stateTiling.tilingWidth = 2;
    stateTiling.tilingHeight = 8;
    getTestModesTiling(states, stateTiling);
    stateTiling.tilingWidth = 8;
    stateTiling.tilingHeight = 2;
    getTestModesTiling(states, stateTiling);
    stateTiling.tilingWidth = 4;
    stateTiling.tilingHeight = 4;
    getTestModesTiling(states, stateTiling);
    stateTiling.tilingWidth = 8;
    stateTiling.tilingHeight = 8;
    getTestModesTiling(states, stateTiling);
    stateTiling.tilingWidth = 8;
    stateTiling.tilingHeight = 8;
    stateTiling.useMortonCodeForTiling = true;
    getTestModesTiling(states, stateTiling);

    // Performance test: No synchronization operations
    InternalState stateNoSync = state;
    stateNoSync.testNoInvocationInterlock = true;
    stateNoSync.testNoAtomicOperations = true;
    getTestModesNoSync(states, stateNoSync);

    // Performance test: Unordered pixel sync
    InternalState stateUnorderedSync = state;
    stateUnorderedSync.testPixelSyncUnordered = true;
    getTestModesUnorderedSync(states, stateUnorderedSync);

    // Quality test: Shuffle geometry randomly
    if (state.modelName == "Aneurism (Lines)") {
        InternalState stateShuffleGeometry = state;
        stateShuffleGeometry.testShuffleGeometry = true;
        getTestModesShuffleGeometry(states, stateShuffleGeometry, 1);
        getTestModesShuffleGeometry(states, stateShuffleGeometry, 2);
    }

    // Performance test: Pixel sync vs. atomic operations
    getTestModesPixelSyncVsAtomicOps(states, state);

    // MLAB with buckets
    getTestModesMLABBuckets(states, state);

    // Voxel ray casting
    getTestModesVoxelRaytracing(states, state);

    return states;
}
