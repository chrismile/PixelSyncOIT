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
    AutoPerfMeasurer(std::vector<InternalState> _states, const std::string &_csvFilename,
            std::function<void(const InternalState&)> _newStateCallback);
    ~AutoPerfMeasurer();

    // To be called by the application
    void startMeasure();
    void endMeasure();
    /// Returns false if all modes were tested and the app should terminate.
    bool update(float dt);

    void resolutionChanged(sgl::FramebufferObjectPtr _sceneFramebuffer);

private:
    /// Write out the performance data of "currentState" to "file".
    void writeCurrentModeData();
    /// Switch to the next state in "states".
    void setNextState(bool first = false);

    /// Make screenshot of scene rendering framebuffer
    void saveScreenshot(const std::string &filename);

    std::vector<InternalState> states;
    size_t currentStateIndex;
    InternalState currentState;
    std::function<void(const InternalState&)> newStateCallback; // Application callback

    sgl::TimerGL timerGL;

    CsvWriter file;
    std::string csvFilename;

    // For making screenshots and computing reference metrics
    sgl::FramebufferObjectPtr sceneFramebuffer;
    sgl::BitmapPtr referenceImage; // Rendered using depth peeling
};


#endif //PIXELSYNCOIT_PERFMEASURER_HPP
