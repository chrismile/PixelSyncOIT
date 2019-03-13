//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_PERFMEASURER_HPP
#define PIXELSYNCOIT_PERFMEASURER_HPP

#include <string>
#include <functional>
#include <Graphics/Buffers/FBO.hpp>
#include <Graphics/Texture/Bitmap.hpp>
#include <Graphics/OpenGL/TimerGL.hpp>

#include "CsvWriter.hpp"
#include "InternalState.hpp"

class AutoPerfMeasurer {
public:
    AutoPerfMeasurer(std::vector<InternalState> _states,
                     const std::string &_csvFilename, const std::string &_depthComplexityFilename,
                     std::function<void(const InternalState&)> _newStateCallback);
    ~AutoPerfMeasurer();

    // To be called by the application
    void setInitialFreeMemKilobytes(int initialFreeMemKilobytes);
    void startMeasure(float timeStamp);
    void endMeasure();

    /// Returns false if all modes were tested and the app should terminate.
    bool update(float currentTime);

    /// Called for first frame
    void makeScreenshot();

    void resolutionChanged(sgl::FramebufferObjectPtr _sceneFramebuffer);

    // Called by OIT_DepthComplexity
    void pushDepthComplexityFrame(uint64_t minComplexity, uint64_t maxComplexity, float avgUsed, float avgAll,
            uint64_t totalNumFragments);

    // Called by OIT algorithms
    void setCurrentAlgorithmBufferSizeBytes(size_t numBytes);

private:
    /// Write out the performance data of "currentState" to "file".
    void writeCurrentModeData();
    /// Switch to the next state in "states".
    void setNextState(bool first = false);

    /// Make screenshot of scene rendering framebuffer
    void saveScreenshot(const std::string &filename);

    /// Returns amount of used video memory size in gigabytes
    float getUsedVideoMemorySizeGB();



    std::vector<InternalState> states;
    size_t currentStateIndex;
    InternalState currentState;
    std::function<void(const InternalState&)> newStateCallback; // Application callback

    sgl::TimerGL timerGL;
    int initialFreeMemKilobytes;

    CsvWriter file;
    CsvWriter depthComplexityFile;
    size_t depthComplexityFrameNumber = 0;
    size_t currentAlgorithmsBufferSizeBytes = 0;

    // For making screenshots and computing reference metrics
    sgl::FramebufferObjectPtr sceneFramebuffer;
    sgl::BitmapPtr referenceImage; // Rendered using depth peeling
};


#endif //PIXELSYNCOIT_PERFMEASURER_HPP
