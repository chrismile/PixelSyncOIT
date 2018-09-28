//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_PERFMEASURER_HPP
#define PIXELSYNCOIT_PERFMEASURER_HPP

#include <string>
#include <functional>
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

private:
    /// Write out the performance data of "currentState" to "file".
    void writeCurrentModeData();
    /// Switch to the next state in "states".
    void setNextState(bool first = false);

    std::vector<InternalState> states;
    size_t currentStateIndex;
    InternalState currentState;
    std::function<void(const InternalState&)> newStateCallback; // Application callback

    sgl::TimerGL timerGL;

    CsvWriter file;
    std::string csvFilename;
};


#endif //PIXELSYNCOIT_PERFMEASURER_HPP
