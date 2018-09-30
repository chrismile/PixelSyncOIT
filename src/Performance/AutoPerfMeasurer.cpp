//
// Created by christoph on 27.09.18.
//

#include <GL/glew.h>

#include <Utils/File/Logfile.hpp>
#include <Utils/AppSettings.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/Texture/Bitmap.hpp>

#include "ReferenceMetric.hpp"
#include "AutoPerfMeasurer.hpp"

AutoPerfMeasurer::AutoPerfMeasurer(std::vector<InternalState> _states, const std::string &_csvFilename,
                                   std::function<void(const InternalState&)> _newStateCallback)
        : states(_states), currentStateIndex(0), newStateCallback(_newStateCallback), file(_csvFilename),
          csvFilename(_csvFilename)
{
    // Write header
    file.writeRow({"Name", "Average Time (ms)", "Image Filename", "SSIM", "RMSE", "PSNR"});

    // Set initial state
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

    // Make screenshot of rendering result with current algorithm
    std::string filename = std::string() + "images/" + currentState.name + ".png";
    file.writeCell(filename);
    saveScreenshot(filename);
    sgl::BitmapPtr image(new sgl::Bitmap());
    image->fromFile(filename.c_str());
    if (currentState.oitAlgorithm == RENDER_MODE_OIT_DEPTH_PEELING) {
        referenceImage = image;
    }

    // If a reference image is saved: Compute reference metrics (SSIM, RMSE, PSNR)
    if (referenceImage) {
        double ssimMetric = ssim(referenceImage, image);
        double rmseMetric = rmse(referenceImage, image);
        double psnrMetric = psnr(referenceImage, image);
        file.writeCell(sgl::toString(ssimMetric));
        file.writeCell(sgl::toString(rmseMetric));
        file.writeCell(sgl::toString(psnrMetric));
    }

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


void AutoPerfMeasurer::resolutionChanged(sgl::FramebufferObjectPtr _sceneFramebuffer)
{
    sceneFramebuffer = _sceneFramebuffer;
}

void AutoPerfMeasurer::saveScreenshot(const std::string &filename)
{
    sgl::Window *window = sgl::AppSettings::get()->getMainWindow();
    int width = window->getWidth();
    int height = window->getHeight();

    sgl::Renderer->bindFBO(sceneFramebuffer);
    sgl::BitmapPtr bitmap(new sgl::Bitmap(width, height, 32));
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->getPixels());
    bitmap->savePNG(filename.c_str(), true);
    sgl::Renderer->unbindFBO();
}