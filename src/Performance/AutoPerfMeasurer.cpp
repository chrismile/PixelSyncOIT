//
// Created by christoph on 27.09.18.
//

#include <Utils/File/Logfile.hpp>

#include "AutoPerfMeasurer.hpp"

AutoPerfMeasurer::AutoPerfMeasurer(std::vector<InternalState> _states, const std::string &_csvFilename,
                                   std::function<void(const InternalState&)> _newStateCallback)
        : states(_states), currentStateIndex(0), newStateCallback(_newStateCallback), file(_csvFilename),
          csvFilename(_csvFilename)
{
    setNextState(true);
}

AutoPerfMeasurer::~AutoPerfMeasurer()
{
    writeCurrentModeData();
    file.close();
}


float nextModeCounter = 0.0f;
const float TIME_PER_MODE = 10.0f; // in seconds
bool AutoPerfMeasurer::update(float dt)
{
    nextModeCounter += dt;
    if (nextModeCounter >= TIME_PER_MODE) {
        nextModeCounter = 0.0f;
        if (currentStateIndex == states.size()-1) {
            return false; // Terminate program
        }
        setNextState();
    }
    return true;
}

void AutoPerfMeasurer::writeCurrentModeData()
{
    // Write row with performance metrics of this mode
    double timeMS = timerGL.getTimeMS(currentState.name);
    file.writeCell(currentState.name);
    file.writeCell(sgl::toString(timeMS));
    file.newRow();
}

void AutoPerfMeasurer::setNextState(bool first)
{
    if (!first) {
        writeCurrentModeData();
        currentStateIndex++;
    }

    currentState = states.at(currentStateIndex);
    sgl::Logfile::get()->writeInfo(std::string() + "New state: " + currentState.name);
    newStateCallback(currentState);
}

void AutoPerfMeasurer::startMeasure()
{
    timerGL.start(currentState.name);
}

void AutoPerfMeasurer::endMeasure()
{
    timerGL.end();
}

